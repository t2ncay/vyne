#===============================================================================
# CrediTrack AI - Vyne Implementation 
#===============================================================================

ruleset {  dynamic_casting };

module vmath;
module vmem;
module vfs;
module vcv;

interface AcademicProfile {
    gpa :: Float64,
    uni_rating :: Float64,
    alpha :: Float64
}

interface FinancialProfile {
    income :: Float64,
    expense :: Float64,
    ach_score :: Float64,
    penalty :: Float64
}

module CreditEngine;

fn :: CreditEngine calc_academic_factor(profile :: AcademicProfile) -> Float64 {
    # Düstur: min(100, G * (1 + alpha * (U / 100)))
    base_factor :: Float64 = profile.gpa * (1.0 + profile.alpha * (profile.uni_rating / 100.0));
    
    if base_factor > 100.0 {
        return 100.0;
    }
    return base_factor;
}

fn :: CreditEngine calc_final_score(acad_factor :: Float64, fin :: FinancialProfile) -> Int64 {
    income_score :: Float64 = 0.0;
    if fin.income > 0.0 {
        income_score = ((fin.income - fin.expense) / fin.income) * 100.0;
    }
    
    w_acad = 0.45;
    w_inc = 0.30;
    w_ach = 0.15;
    w_course = 0.10;
    
    weighted_sum = (acad_factor * w_acad) + (income_score * w_inc) + (fin.ach_score * w_ach) + (60.0 * w_course);
    
    # R_score düsturu: 300 + (weighted_sum * 5.5) * P
    final_raw = 300.0 + (weighted_sum * 5.5) * fin.penalty;
    
    return int64(final_raw);
}

#===============================================================================
# MAIN EXECUTION (Test Case)
#===============================================================================

out("=== CrediTrack Vyne Engine ===");

tuncay_acad :: AcademicProfile = AcademicProfile(92.5, 85.0, 0.25);
tuncay_fin :: FinancialProfile = FinancialProfile(1200.0, 400.0, 90.0, 1.0);

s_acad = CreditEngine.calc_academic_factor(tuncay_acad);
r_score = CreditEngine.calc_final_score(s_acad, tuncay_fin);

out("Student: Tuncay");
out("Academic Factor (S_acad): " + string(s_acad));
out("Final Credit Score (R_score): " + string(r_score));

# segmentation
risk_segments = ["High Risk", "Medium Risk", "Low Risk"];
status = "";

if r_score >= 680 {
    status = risk_segments[2];
} else if r_score >= 500 {
    status = risk_segments[1];
} else {
    status = risk_segments[0];
}

out("Financial Status: " + status);