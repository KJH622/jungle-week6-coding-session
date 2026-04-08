const templates = {
  insert: [
    "INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');",
    "INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');"
  ].join("\n"),
  select: "SELECT name, age FROM users;",
  error: "SELECT unknown_col FROM users;"
};

const schemas = {
  users: ["name", "age", "major"],
  products: ["name", "category", "price"]
};

const PAGE_SIZE_BYTES = 256;
const PAGE_HEADER_BYTES = 24;
const ROW_OVERHEAD_BYTES = 6;
const textEncoder = new TextEncoder();

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

let db = createEmptyDb();
let cacheStats = createEmptyCacheStats();
let lastAccessTable = null;

function createEmptyDb() {
  return {
    users: [],
    products: []
  };
}

function createEmptyCacheStats() {
  return {
    hit: 0,
    miss: 0,
    dirtyPages: 0,
    flushCount: 0
  };
}

function renderCacheStats() {
  if (cacheHitEl) cacheHitEl.textContent = String(cacheStats.hit);
  if (cacheMissEl) cacheMissEl.textContent = String(cacheStats.miss);
  if (dirtyPagesEl) dirtyPagesEl.textContent = String(cacheStats.dirtyPages);
  if (flushCountEl) flushCountEl.textContent = String(cacheStats.flushCount);
}

function byteLength(value) {
  return textEncoder.encode(String(value)).length;
}

function estimateRowBytes(row, schema) {
  let total = ROW_OVERHEAD_BYTES;

  for (const col of schema) {
    total += byteLength(col) + 1;
    total += byteLength(row[col] ?? "");
  }

  return total;
}

function calculatePageStats(table) {
  const rows = db[table] || [];
  const schema = schemas[table] || [];

  if (rows.length === 0) {
    return {
      pageCount: 0,
      rowCount: 0,
      lastUsedBytes: 0
    };
  }

  let pageCount = 1;
  let usedBytes = PAGE_HEADER_BYTES;

  for (const row of rows) {
    const rowBytes = estimateRowBytes(row, schema);

    if (usedBytes + rowBytes > PAGE_SIZE_BYTES) {
      pageCount += 1;
      usedBytes = PAGE_HEADER_BYTES;
    }

    usedBytes += rowBytes;
  }

  return {
    pageCount,
    rowCount: rows.length,
    lastUsedBytes: usedBytes
  };
}

function renderPageStats() {
  const usersStats = calculatePageStats("users");
  const productsStats = calculatePageStats("products");

  if (usersPageCountEl) usersPageCountEl.textContent = String(usersStats.pageCount);
  if (usersRowCountEl) usersRowCountEl.textContent = String(usersStats.rowCount);
  if (usersLastUsedBytesEl) usersLastUsedBytesEl.textContent = String(usersStats.lastUsedBytes);

  if (productsPageCountEl) productsPageCountEl.textContent = String(productsStats.pageCount);
  if (productsRowCountEl) productsRowCountEl.textContent = String(productsStats.rowCount);
  if (productsLastUsedBytesEl) productsLastUsedBytesEl.textContent = String(productsStats.lastUsedBytes);
}

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

function isIdentifier(value) {
  return /^[A-Za-z_][A-Za-z0-9_]*$/.test(value);
}

function splitStatements(sqlText) {
  const statements = [];
  let inQuotes = false;
  let start = 0;

  for (let i = 0; i < sqlText.length; i += 1) {
    const ch = sqlText[i];

    if (ch === "'") {
      if (inQuotes && sqlText[i + 1] === "'") {
        i += 1;
        continue;
      }
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && ch === ";") {
      const part = sqlText.slice(start, i).trim();
      if (part) {
        statements.push(part);
      }
      start = i + 1;
    }
  }

  if (inQuotes) {
    throw new Error("invalid query");
  }

  const tail = sqlText.slice(start).trim();
  if (tail) {
    statements.push(tail);
  }

  return statements;
}

function splitCommaList(text) {
  const items = [];
  let inQuotes = false;
  let tokenStart = 0;

  for (let i = 0; i <= text.length; i += 1) {
    const ch = text[i];

    if (ch === "'") {
      if (inQuotes && text[i + 1] === "'") {
        i += 1;
        continue;
      }
      inQuotes = !inQuotes;
      continue;
    }

    if ((ch === "," && !inQuotes) || i === text.length) {
      const token = text.slice(tokenStart, i).trim();
      if (!token) {
        throw new Error("invalid query");
      }
      items.push(token);
      tokenStart = i + 1;
    }
  }

  if (inQuotes) {
    throw new Error("invalid query");
  }

  return items;
}

function decodeValue(raw) {
  const value = raw.trim();

  if (value.startsWith("'") && value.endsWith("'") && value.length >= 2) {
    const inner = value.slice(1, -1);
    return inner.replace(/''/g, "'");
  }

  return value;
}

function parseInsert(statement) {
  const match = statement.match(/^\s*insert\s+into\s+([A-Za-z_][A-Za-z0-9_]*)\s*\((.+)\)\s*values\s*\((.+)\)\s*$/i);
  if (!match) {
    throw new Error("invalid query");
  }

  const table = match[1].toLowerCase();
  const columns = splitCommaList(match[2]).map((item) => item.toLowerCase());
  const values = splitCommaList(match[3]).map(decodeValue);

  return { type: "insert", table, columns, values };
}

function parseSelect(statement) {
  const match = statement.match(/^\s*select\s+(.+?)\s+from\s+([A-Za-z_][A-Za-z0-9_]*)\s*$/i);
  if (!match) {
    throw new Error("invalid query");
  }

  const columnPart = match[1].trim();
  const table = match[2].toLowerCase();
  const selectAll = columnPart === "*";
  const columns = selectAll ? [] : splitCommaList(columnPart).map((item) => item.toLowerCase());

  return { type: "select", table, selectAll, columns };
}

function parseStatement(statement) {
  const lower = statement.trim().toLowerCase();

  if (lower.startsWith("insert")) {
    return parseInsert(statement);
  }

  if (lower.startsWith("select")) {
    return parseSelect(statement);
  }

  throw new Error("invalid query");
}

function executeInsert(ast) {
  const schema = schemas[ast.table];
  if (!schema) {
    throw new Error("table not found");
  }

  if (ast.columns.length !== ast.values.length || ast.columns.length !== schema.length) {
    throw new Error("column count does not match value count");
  }

  const row = {};
  const assigned = new Set();
  const beforePageStats = calculatePageStats(ast.table);

  for (let i = 0; i < ast.columns.length; i += 1) {
    const col = ast.columns[i];

    if (!isIdentifier(col) || !schema.includes(col) || assigned.has(col)) {
      throw new Error("invalid query");
    }

    row[col] = ast.values[i];
    assigned.add(col);
  }

  db[ast.table].push(row);

  const afterPageStats = calculatePageStats(ast.table);
  if (afterPageStats.pageCount > beforePageStats.pageCount) {
    addEventLog(`page full -> create page#${afterPageStats.pageCount - 1} (${ast.table})`);
  }

  cacheStats.miss += 1;
  cacheStats.dirtyPages = Math.max(1, cacheStats.dirtyPages);
  lastAccessTable = ast.table;

  renderCacheStats();
  renderPageStats();
  addEventLog(`APPEND row -> ${ast.table} (used_bytes=${afterPageStats.lastUsedBytes})`);
}

function executeSelect(ast, stdoutLines) {
  const schema = schemas[ast.table];
  if (!schema) {
    throw new Error("table not found");
  }

  const columns = ast.selectAll ? schema : ast.columns;

  for (const col of columns) {
    if (!isIdentifier(col) || !schema.includes(col)) {
      throw new Error("invalid query");
    }
  }

  if (lastAccessTable === ast.table) {
    cacheStats.hit += 1;
    addEventLog(`LOAD ${ast.table} -> cache hit`);
  } else {
    cacheStats.miss += 1;
    addEventLog(`LOAD ${ast.table} -> cache miss`);
  }
  lastAccessTable = ast.table;

  const rows = db[ast.table];
  const stats = calculatePageStats(ast.table);

  if (rows.length === 0) {
    renderCacheStats();
    renderPageStats();
    addEventLog(`SCAN ${ast.table} -> 0 rows`);
    return;
  }

  stdoutLines.push(columns.join(","));

  for (const row of rows) {
    const line = columns.map((col) => row[col] ?? "").join(",");
    stdoutLines.push(line);
  }

  renderCacheStats();
  renderPageStats();
  addEventLog(`SCAN ${ast.table} -> ${rows.length} rows, ${stats.pageCount} pages`);
}

function flushIfNeeded() {
  if (cacheStats.dirtyPages > 0) {
    cacheStats.flushCount += 1;
    addEventLog(`FLUSH dirty pages -> ${cacheStats.dirtyPages}`);
    cacheStats.dirtyPages = 0;
    renderCacheStats();
  }
}

function runSql() {
  const sql = sqlInput.value;
  const stdoutLines = [];
  const stderrLines = [];

  addEventLog("RUN SQL requested");

  try {
    const statements = splitStatements(sql);
    addEventLog(`PARSE statements -> ${statements.length}`);

    for (const statement of statements) {
      try {
        const ast = parseStatement(statement);

        if (ast.type === "insert") {
          executeInsert(ast);
        } else if (ast.type === "select") {
          executeSelect(ast, stdoutLines);
        } else {
          throw new Error("invalid query");
        }
      } catch (err) {
        stderrLines.push(`ERROR: ${err.message || "invalid query"}`);
        addEventLog(`ERROR -> ${err.message || "invalid query"}`);
      }
    }
  } catch (err) {
    stderrLines.push(`ERROR: ${err.message || "invalid query"}`);
    addEventLog(`ERROR -> ${err.message || "invalid query"}`);
  }

  flushIfNeeded();
  renderPageStats();

  stdoutOutput.textContent = stdoutLines.join("\n");
  stderrOutput.textContent = stderrLines.join("\n");

  if (stderrLines.length > 0) {
    showToast("SQL 실행 완료: 에러가 포함되어 있습니다.", "warn");
    addEventLog("RUN complete (with error)");
  } else {
    showToast("SQL 실행 완료", "success");
    addEventLog("RUN complete (success)");
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

runBtn.addEventListener("click", runSql);

sqlInput.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
    event.preventDefault();
    runSql();
  }
});

clearBtn.addEventListener("click", () => {
  stdoutOutput.textContent = "";
  stderrOutput.textContent = "";
  showToast("출력을 비웠습니다.", "info");
  addEventLog("OUTPUT cleared");
});

resetBtn.addEventListener("click", () => {
  db = createEmptyDb();
  cacheStats = createEmptyCacheStats();
  lastAccessTable = null;
  renderCacheStats();
  renderPageStats();
  stdoutOutput.textContent = "";
  stderrOutput.textContent = "";
  showToast("데이터를 초기화했습니다.", "warn");
  addEventLog("DATA reset");
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
renderCacheStats();
renderPageStats();
addEventLog("UI ready");
showToast("SQL을 입력하고 Run SQL을 눌러 실행하세요.", "info");
