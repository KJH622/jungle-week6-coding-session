const templates = {
  insert: [
    "INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');",
    "INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');",
    "INSERT INTO users (name, age, major) VALUES ('SELECT', 28, '데이터사이언스');",
    "",
    "INSERT INTO products (name, category, price) VALUES ('맥북프로', '노트북', 3200000);",
    "INSERT INTO products (name, category, price) VALUES ('무선마우스', '주변기기', 39000);",
    "INSERT INTO products (name, category, price) VALUES ('할인키보드', '주변기기', -5000);"
  ].join("\n"),
  select: [
    "SELECT * FROM users;",
    "SELECT name, age FROM users;",
    "",
    "SELECT * FROM products;",
    "SELECT name, price FROM products;",
    "",
    "SELECT name FROM users;",
    "SELECT category FROM products;"
  ].join("\n"),
  error: "SELECT unknown_col FROM users;"
};

const buttons = Array.from(document.querySelectorAll(".demo-btn"));
const sqlInput = document.getElementById("sqlInput");
const stdoutOutput = document.getElementById("stdoutOutput");
const stderrOutput = document.getElementById("stderrOutput");
const runBtn = document.getElementById("runBtn");
const clearBtn = document.getElementById("clearBtn");
const resetBtn = document.getElementById("resetBtn");
const clearLogBtn = document.getElementById("clearLogBtn");
const toastContainer = document.getElementById("toastContainer");
const eventLogList = document.getElementById("eventLogList");

const cacheHitEl = document.getElementById("cacheHit");
const cacheMissEl = document.getElementById("cacheMiss");
const dirtyPagesEl = document.getElementById("dirtyPages");
const flushCountEl = document.getElementById("flushCount");

const usersPageCountEl = document.getElementById("usersPageCount");
const usersRowCountEl = document.getElementById("usersRowCount");
const usersLastUsedBytesEl = document.getElementById("usersLastUsedBytes");
const productsPageCountEl = document.getElementById("productsPageCount");
const productsRowCountEl = document.getElementById("productsRowCount");
const productsLastUsedBytesEl = document.getElementById("productsLastUsedBytes");

function nowTime() {
  const d = new Date();
  return d.toLocaleTimeString("ko-KR", { hour12: false });
}

function addEventLog(message) {
  if (!eventLogList) {
    return;
  }

  const item = document.createElement("li");
  item.className = "event-log-item";
  item.innerHTML = `<span class="log-time">[${nowTime()}]</span>${message}`;

  eventLogList.prepend(item);

  while (eventLogList.children.length > 20) {
    eventLogList.removeChild(eventLogList.lastElementChild);
  }
}

function showToast(message, type = "info") {
  if (!toastContainer) {
    return;
  }

  const toast = document.createElement("div");
  toast.className = `toast ${type}`;
  toast.textContent = message;
  toastContainer.appendChild(toast);

  setTimeout(() => {
    toast.style.opacity = "0";
    toast.style.transform = "translateY(-4px)";
    setTimeout(() => toast.remove(), 180);
  }, 1800);
}

function setText(el, value) {
  if (el) {
    el.textContent = String(value);
  }
}

function renderRuntimeState(state, connected) {
  const cache = state?.cacheStats;
  if (connected && cache) {
    setText(cacheHitEl, cache.hit ?? 0);
    setText(cacheMissEl, cache.miss ?? 0);
    setText(dirtyPagesEl, cache.dirtyPages ?? 0);
    setText(flushCountEl, cache.flushCount ?? 0);
  } else {
    setText(cacheHitEl, "-");
    setText(cacheMissEl, "-");
    setText(dirtyPagesEl, "-");
    setText(flushCountEl, "-");
  }

  const users = state?.dataFiles?.users;
  const products = state?.dataFiles?.products;

  setText(usersPageCountEl, users?.pageCount ?? 0);
  setText(usersRowCountEl, users?.rowCount ?? 0);
  setText(usersLastUsedBytesEl, users?.lastPageUsedBytes ?? 0);

  setText(productsPageCountEl, products?.pageCount ?? 0);
  setText(productsRowCountEl, products?.rowCount ?? 0);
  setText(productsLastUsedBytesEl, products?.lastPageUsedBytes ?? 0);
}

async function fetchJson(url, options = {}) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));

  if (!response.ok) {
    const error = new Error(data.error || `request failed (${response.status})`);
    error.payload = data;
    throw error;
  }

  return data;
}

async function refreshState() {
  try {
    const data = await fetchJson("/api/state");
    renderRuntimeState(data, true);
    return data;
  } catch (error) {
    renderRuntimeState(null, false);
    addEventLog(`API offline -> ${error.message}`);
    throw error;
  }
}

async function runSql() {
  const sql = sqlInput.value;

  addEventLog("RUN SQL requested");
  runBtn.disabled = true;
  runBtn.textContent = "Running...";

  try {
    const data = await fetchJson("/api/run-sql", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({ sql })
    });

    stdoutOutput.textContent = data.stdout || "";
    stderrOutput.textContent = data.stderr || "";
    renderRuntimeState(data.state, true);
    addEventLog(`PROCESS exit -> ${data.exitCode}`);

    if (data.stderr) {
      showToast("실제 C 프로그램 실행 완료: 에러가 포함되어 있습니다.", "warn");
      addEventLog("RUN complete (with error)");
    } else {
      showToast("실제 C 프로그램 실행 완료", "success");
      addEventLog("RUN complete (success)");
    }
  } catch (error) {
    stderrOutput.textContent = error.payload?.stderr || error.message;
    showToast("API 실행에 실패했습니다.", "warn");
    addEventLog(`API ERROR -> ${error.message}`);

    try {
      await refreshState();
    } catch (refreshError) {
      void refreshError;
    }
  } finally {
    runBtn.disabled = false;
    runBtn.textContent = "Run SQL";
  }
}

function setTemplate(name) {
  sqlInput.value = templates[name];
}

buttons.forEach((button) => {
  button.addEventListener("click", () => {
    buttons.forEach((btn) => btn.classList.remove("is-active"));
    button.classList.add("is-active");
    setTemplate(button.dataset.scenario);
    showToast("템플릿을 불러왔습니다.", "info");
    addEventLog(`TEMPLATE -> ${button.dataset.scenario}`);
  });
});

runBtn.addEventListener("click", () => {
  void runSql();
});

sqlInput.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
    event.preventDefault();
    void runSql();
  }
});

clearBtn.addEventListener("click", () => {
  stdoutOutput.textContent = "";
  stderrOutput.textContent = "";
  showToast("출력을 비웠습니다.", "info");
  addEventLog("OUTPUT cleared");
});

resetBtn.addEventListener("click", async () => {
  resetBtn.disabled = true;

  try {
    const data = await fetchJson("/api/reset-data", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: "{}"
    });

    stdoutOutput.textContent = "";
    stderrOutput.textContent = "";
    renderRuntimeState(data.state, true);
    showToast("실제 데이터 파일을 초기화했습니다.", "warn");
    addEventLog("DATA reset");
  } catch (error) {
    showToast("데이터 초기화에 실패했습니다.", "warn");
    addEventLog(`RESET ERROR -> ${error.message}`);
  } finally {
    resetBtn.disabled = false;
  }
});

if (clearLogBtn) {
  clearLogBtn.addEventListener("click", () => {
    if (eventLogList) {
      eventLogList.innerHTML = "";
    }
    addEventLog("EVENT LOG cleared");
    showToast("이벤트 로그를 비웠습니다.", "info");
  });
}

setTemplate("insert");
addEventLog("UI ready");
showToast("로컬 API 서버에 연결해 실제 C 프로그램을 실행합니다.", "info");

void refreshState()
  .then(() => {
    addEventLog("API connected");
  })
  .catch(() => {
    showToast("API 서버를 먼저 실행하세요: python3 member-jihyun/demo-ui/server.py", "warn");
  });
