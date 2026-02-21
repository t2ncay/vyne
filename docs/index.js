// Mobile menu toggle
const mobileMenuBtn = document.getElementById("mobileMenuBtn");
const sidebar = document.getElementById("sidebar");

mobileMenuBtn.addEventListener("click", () => {
  sidebar.classList.toggle("active");
});

// Close sidebar when clicking outside on mobile
document.addEventListener("click", (event) => {
  const isMobile = window.innerWidth <= 768;
  if (
    isMobile &&
    !sidebar.contains(event.target) &&
    !mobileMenuBtn.contains(event.target)
  ) {
    sidebar.classList.remove("active");
  }
});

// Theme toggle functionality
const themeToggle = document.getElementById("themeToggle");
const themeIcon = themeToggle.querySelector("i");
const themeText = themeToggle.querySelector("span");

themeToggle.addEventListener("click", () => {
  document.body.classList.toggle("light-theme");

  if (document.body.classList.contains("light-theme")) {
    // Switch to light theme
    document.documentElement.style.setProperty("--bg-dark", "#f8f9fa");
    document.documentElement.style.setProperty("--bg-card", "#ffffff");
    document.documentElement.style.setProperty("--bg-sidebar", "#e9ecef");
    document.documentElement.style.setProperty("--text-primary", "#212529");
    document.documentElement.style.setProperty("--text-secondary", "#495057");
    document.documentElement.style.setProperty("--border-color", "#b5bac0");
    document.documentElement.style.setProperty(
      "--example-bg",
      "rgba(0, 0, 0, 0.03)",
    );
    document.documentElement.style.setProperty("--code-bg", "#f1f3f5");
    document.getElementById("vyneLogo").src = "assets/lightModeVyneLogo.png";
    document.getElementById("vyneLogo").style = "width:100px; height: 100px";
    themeIcon.className = "fas fa-moon";
    themeText.textContent = "Dark Mode";
  } else {
    // Switch to dark theme (default)
    document.documentElement.style.setProperty("--bg-dark", "#2B3033");
    document.documentElement.style.setProperty("--bg-card", "#3e4247");
    document.documentElement.style.setProperty("--bg-sidebar", "#242a2d");
    document.documentElement.style.setProperty("--text-primary", "#dee7ef");
    document.documentElement.style.setProperty("--text-secondary", "#adb5bd");
    document.documentElement.style.setProperty("--border-color", "#545d6d");
    document.documentElement.style.setProperty(
      "--example-bg",
      "rgba(255, 255, 255, 0.03)",
    );
    document.documentElement.style.setProperty("--code-bg", "#141920");
    document.getElementById("vyneLogo").src = "assets/sidebarVyneLogo.png";
    themeIcon.className = "fas fa-sun";
    themeText.textContent = "Light Mode";
  }
});

// Search functionality
const searchInput = document.getElementById("searchInput");
const navLinks = document.querySelectorAll(".nav-links a");

searchInput.addEventListener("input", (e) => {
  const searchTerm = e.target.value.toLowerCase();

  navLinks.forEach((link) => {
    const text = link.textContent.toLowerCase();
    const parentLi = link.parentElement;

    if (text.includes(searchTerm)) {
      parentLi.style.display = "block";
    } else {
      parentLi.style.display = "none";
    }
  });
});

// Active navigation link highlighting
const sections = document.querySelectorAll("section");
const navLinksArray = Array.from(navLinks);

function updateActiveNavLink() {
  let currentSectionId = "";

  sections.forEach((section) => {
    const sectionTop = section.offsetTop - 100;
    const sectionHeight = section.clientHeight;
    const scrollPosition = window.scrollY;

    if (
      scrollPosition >= sectionTop &&
      scrollPosition < sectionTop + sectionHeight
    ) {
      currentSectionId = section.id;
    }
  });

  navLinksArray.forEach((link) => {
    link.classList.remove("active");
    if (link.getAttribute("href") === `#${currentSectionId}`) {
      link.classList.add("active");
    }
  });
}

window.addEventListener("scroll", updateActiveNavLink);

// Smooth scrolling for anchor links
document.querySelectorAll('a[href^="#"]').forEach((anchor) => {
  anchor.addEventListener("click", function (e) {
    e.preventDefault();

    const targetId = this.getAttribute("href");
    if (targetId === "#") return;

    const targetElement = document.querySelector(targetId);
    if (targetElement) {
      window.scrollTo({
        top: targetElement.offsetTop - 80,
        behavior: "smooth",
      });

      // Close mobile menu after clicking a link
      if (window.innerWidth <= 768) {
        sidebar.classList.remove("active");
      }
    }
  });
});

// Copy code button functionality
window.copyCode = function (button) {
  const codeBlock = button.closest(".code");
  const codeElement = codeBlock.querySelector("code");
  const textToCopy = codeElement.textContent;

  navigator.clipboard.writeText(textToCopy).then(() => {
    const originalText = button.innerHTML;
    button.textContent = "Copied!";
    button.style.backgroundColor = "var(--success-color)";
    console.log(originalText);
    setTimeout(() => {
      button.innerHTML = originalText;
      button.style.backgroundColor = "";
    }, 2000);
  });
};

// Initialize syntax highlighting (simple implementation)
function highlightCodeBlocks() {
  const codeBlocks = document.querySelectorAll("code");

  codeBlocks.forEach((block) => {
    // 1. Skip if already highlighted or if it's bash
    if (
      block.classList.contains("language-bash") ||
      block.getAttribute("data-highlighted")
    ) {
      return;
    }

    let code = block.textContent;

    // 2. Process the string (The order matters!)
    // Strings
    code = code.replace(
      /(["'])(?:(?=(\\?))\2.)*?\1/g,
      '<span class="token string">$&</span>',
    );

    // Keywords - using strict word boundaries

    const kwRegex =
      /\b(fn|group|module|if|else|through|as|return|var|const|while|out|use|extern|)\b/g;
    code = code.replace(kwRegex, '<span class="token keyword">$1</span>');

    // Numbers
    code = code.replace(/\b\d+\b/g, '<span class="token number">$&</span>');

    // $ Operator
    code = code.replace(/(\$)/g, '<span class="token operator">$1</span>');

    // 3. Set the HTML and mark it so it doesn't run again
    block.innerHTML = code;
    block.setAttribute("data-highlighted", "true");
  });
}

// Call highlight function on page load
document.addEventListener("DOMContentLoaded", highlightCodeBlocks);

// Add active class to current section on page load
updateActiveNavLink();
