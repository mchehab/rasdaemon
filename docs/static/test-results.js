(() => {
  const root = document.documentElement;
  const themeButton = document.getElementById("theme");
  const themes = ["auto", "light", "dark"];

  function applyTheme(theme) {
    if (theme === "auto") root.removeAttribute("data-theme");
    else root.dataset.theme = theme;
    const labels = { auto: "◐ Auto", light: "☀ Normal", dark: "☾ Nocturn" };
    themeButton.textContent = labels[theme];
    localStorage.setItem("rasdaemon-theme", theme);
  }

  let theme = localStorage.getItem("rasdaemon-theme") || "auto";
  if (!themes.includes(theme)) theme = "auto";
  applyTheme(theme);
  themeButton.addEventListener("click", () => {
    theme = themes[(themes.indexOf(theme) + 1) % themes.length];
    applyTheme(theme);
  });

  const filter = document.getElementById("filter");
  if (filter) filter.addEventListener("input", event => {
    const value = event.target.value.toLowerCase();
    document.querySelectorAll("#results tbody tr").forEach(row => {
      row.hidden = !row.textContent.toLowerCase().includes(value);
    });
  });
})();
