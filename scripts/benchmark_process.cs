// Host-only process measurements. No generated-runtime instrumentation.
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

public sealed class VyneBenchmarkRun
{
    public double WallSec, LaunchSec, ExitObservedSec;
    public double? ProcessLifetimeSec, CpuSec;
    public long? SampledWorkingSetBytes, ProcessPeakWorkingSetBytes, ProcessPeakPagefileBytes;
    public int Samples, ExitCode;
    public bool TimedOut;
    public string TimingSource, SampledWorkingSetStatus, ProcessPeakStatus, CpuStatus;
    public string Stdout, Stderr;
}

public static class VyneBenchmarkProcess
{
    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryCounters
    {
        public uint Size, PageFaultCount;
        public UIntPtr PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage,
            QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage,
            PagefileUsage, PeakPagefileUsage;
    }

    [DllImport("psapi.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetProcessMemoryInfo(IntPtr process, ref MemoryCounters counters, uint size);

    public static VyneBenchmarkRun Run(string path, int intervalMs, int timeoutSec)
    {
        var result = new VyneBenchmarkRun {
            TimingSource = "monotonic-exit-observed", SampledWorkingSetStatus = "missed",
            ProcessPeakStatus = "unsupported", CpuStatus = "unavailable"
        };
        using (var process = new Process())
        {
            process.StartInfo = new ProcessStartInfo(path) {
                UseShellExecute = false, RedirectStandardOutput = true,
                RedirectStandardError = true, CreateNoWindow = true,
                WorkingDirectory = System.IO.Path.GetDirectoryName(path)
            };
            bool windows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
            var timer = Stopwatch.StartNew();
            process.Start();
            result.LaunchSec = timer.Elapsed.TotalSeconds;
            // Hold the Windows process handle until post-exit accounting is read.
            IntPtr handle = windows ? process.Handle : IntPtr.Zero;
            Task<string> stdout = process.StandardOutput.ReadToEndAsync();
            Task<string> stderr = process.StandardError.ReadToEndAsync();
            try
            {
                while (!process.HasExited)
                {
                    double remainingMs = timeoutSec * 1000.0 - timer.Elapsed.TotalMilliseconds;
                    if (remainingMs <= 0)
                    {
                        result.TimedOut = true;
                        process.Kill(true);
                        if (!process.WaitForExit(5000))
                            throw new TimeoutException("Benchmark process did not exit after termination.");
                        break;
                    }
                    try
                    {
                        process.Refresh();
                        long ws = process.WorkingSet64;
                        // Some platforms return zero after a raced exit. It is not a sample.
                        if (ws > 0)
                        {
                            result.SampledWorkingSetBytes = Math.Max(result.SampledWorkingSetBytes ?? 0, ws);
                            result.Samples++;
                            result.SampledWorkingSetStatus = "sampled";
                        }
                    }
                    catch (PlatformNotSupportedException) { result.SampledWorkingSetStatus = "unsupported"; }
                    catch (InvalidOperationException) { }
                    catch (System.ComponentModel.Win32Exception) { }
                    // Wait returns at exit, even if the requested sampling interval is large.
                    int waitMs = (int)Math.Max(1, Math.Min(intervalMs,
                        Math.Ceiling(timeoutSec * 1000.0 - timer.Elapsed.TotalMilliseconds)));
                    if (process.WaitForExit(waitMs)) break;
                }
                result.ExitObservedSec = timer.Elapsed.TotalSeconds;
                result.WallSec = result.ExitObservedSec;
                result.ExitCode = result.TimedOut ? -1 : process.ExitCode;
                if (windows)
                {
                    // GetProcessTimes via .NET: OS lifetime, not polling/notification time.
                    try
                    {
                        double lifetime = (process.ExitTime.ToUniversalTime() - process.StartTime.ToUniversalTime()).TotalSeconds;
                        if (lifetime >= 0)
                        {
                            result.ProcessLifetimeSec = lifetime;
                            result.WallSec = lifetime;
                            result.TimingSource = "windows-process-times";
                        }
                    }
                    catch (InvalidOperationException) { }
                    catch (System.ComponentModel.Win32Exception) { }
                    var counters = new MemoryCounters();
                    counters.Size = (uint)Marshal.SizeOf(typeof(MemoryCounters));
                    result.ProcessPeakStatus = "unavailable";
                    if (GetProcessMemoryInfo(handle, ref counters, counters.Size))
                    {
                        result.ProcessPeakWorkingSetBytes = checked((long)counters.PeakWorkingSetSize.ToUInt64());
                        result.ProcessPeakPagefileBytes = checked((long)counters.PeakPagefileUsage.ToUInt64());
                        result.ProcessPeakStatus = "os-reported";
                    }
                }
                try
                {
                    result.CpuSec = process.TotalProcessorTime.TotalSeconds;
                    result.CpuStatus = "os-reported";
                }
                catch (InvalidOperationException) { }
                catch (System.ComponentModel.Win32Exception) { }
                catch (PlatformNotSupportedException) { result.CpuStatus = "unsupported"; }
                // A descendant retaining a redirected pipe must not hang the harness.
                if (!Task.WaitAll(new Task[] { stdout, stderr }, 5000))
                    throw new TimeoutException("Benchmark output did not close within 5 seconds after process exit.");
                result.Stdout = stdout.GetAwaiter().GetResult();
                result.Stderr = stderr.GetAwaiter().GetResult();
                return result;
            }
            finally
            {
                if (!process.HasExited)
                {
                    process.Kill(true);
                    process.WaitForExit(5000);
                }
            }
        }
    }
}
