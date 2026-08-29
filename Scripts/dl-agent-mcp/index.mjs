#!/usr/bin/env node
/**
 * Stdio MCP for the Calling PIE/game agent bridge.
 * Talks only to 127.0.0.1 — the game rejects HTTP from anywhere else.
 * `boot` can spawn UnrealEditor; `director` drives the I-menu overlay.
 */
import http from "node:http";
import readline from "node:readline";
import path from "node:path";
import { existsSync } from "node:fs";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

const HOST = "127.0.0.1";
const PORT = Number(process.env.DL_AGENT_PORT || 18765);
const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO = process.env.DL_REPO || path.resolve(HERE, "../..");
const EDITOR =
  process.env.DL_UE_EDITOR ||
  "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe";
const UPROJECT =
  process.env.DL_UPROJECT || path.join(REPO, "Calling.uproject");
const PLAY_PY = path.join(REPO, "Scripts", "dl-editor-play.py");

function request(method, pathname, body, timeoutMs = 8000) {
  return new Promise((resolve, reject) => {
    const payload = body ? Buffer.from(JSON.stringify(body)) : null;
    const req = http.request(
      {
        host: HOST,
        port: PORT,
        path: pathname,
        method,
        headers: {
          "Content-Type": "application/json",
          ...(payload ? { "Content-Length": payload.length } : {}),
        },
      },
      (res) => {
        const chunks = [];
        res.on("data", (c) => chunks.push(c));
        res.on("end", () => {
          const text = Buffer.concat(chunks).toString("utf8");
          try {
            resolve(JSON.parse(text));
          } catch {
            resolve({ raw: text, status: res.statusCode });
          }
        });
      }
    );
    req.setTimeout(timeoutMs, () => {
      req.destroy();
      reject(new Error("timeout"));
    });
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

async function probeState() {
  try {
    return await request("GET", "/state", null, 2500);
  } catch {
    return null;
  }
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

async function waitForHttp(deadlineMs) {
  const start = Date.now();
  while (Date.now() - start < deadlineMs) {
    const s = await probeState();
    if (s) return s;
    await sleep(1500);
  }
  return null;
}

function launchUnreal(mode) {
  if (!existsSync(EDITOR)) {
    throw new Error(`editor_missing ${EDITOR}`);
  }
  if (!existsSync(UPROJECT)) {
    throw new Error(`uproject_missing ${UPROJECT}`);
  }
  const args =
    mode === "editor"
      ? [UPROJECT, `-ExecutePythonScript=${PLAY_PY}`]
      : [UPROJECT, "-game", "-WINDOWED", "-ResX=1600", "-ResY=900", "-NOSPLASH"];
  const child = spawn(EDITOR, args, {
    detached: true,
    stdio: "ignore",
    windowsHide: false,
  });
  child.unref();
  return { pid: child.pid, args };
}

async function waitForScene(want, ms) {
  const until = Date.now() + ms;
  let state = null;
  while (Date.now() < until) {
    state = (await probeState()) || state;
    if (state?.scene === want) return state;
    await sleep(400);
  }
  return state;
}

async function waitForNavTiles(ms) {
  const until = Date.now() + ms;
  let state = null;
  while (Date.now() < until) {
    state = (await probeState()) || state;
    if (Number(state?.navTiles) > 0) return state;
    await sleep(400);
  }
  return state;
}

/** Race Compose PvP lobby into the match (minPlayers=2). Same path as dl-rebuild.ps1. */
async function completeComposerIntoMatch() {
  await request("POST", "/director", { action: "host" });
  let state = await probeState();
  if (!state?.lobby?.localHost) {
    throw new Error("composer localHost false after host");
  }
  await request("POST", "/director", { action: "ready" });
  await sleep(200);
  state = await probeState();
  const hostSeat = (state?.lobby?.seatList || []).find((s) => s.host);
  if (!hostSeat?.ready) {
    throw new Error("composer host ready did not stick");
  }
  const joinB = await request("POST", "/hub", {
    type: "join",
    displayName: "bootB",
    headless: true,
    kind: "cursor",
  });
  const seatB = joinB.seatId;
  if (!seatB) throw new Error("composer guest join failed");
  await request("POST", "/hub", { type: "setTeam", seatId: seatB, team: "blue" });
  await request("POST", "/hub", { type: "ready", seatId: seatB, ready: true });
  await sleep(200);
  state = await probeState();
  if (Number(state?.lobby?.ready) < 2) {
    throw new Error(`composer expected 2 ready, got ${state?.lobby?.ready}`);
  }
  await request("POST", "/director", { action: "go" });
  state = await waitForScene("pvp", 30000);
  if (state?.scene !== "pvp") {
    throw new Error(`composer go did not reach pvp scene=${state?.scene}`);
  }
  return waitForNavTiles(45000);
}

async function boot(args) {
  const mode = (args.mode || "game").toLowerCase();
  const activity = (args.activity || "pvp").toLowerCase();
  const waitMs = Math.max(15, Number(args.waitSeconds || 90)) * 1000;
  let launched = false;
  let spawnInfo = null;
  let state = await probeState();
  if (!state) {
    spawnInfo = launchUnreal(mode === "editor" ? "editor" : "game");
    launched = true;
    state = await waitForHttp(waitMs);
    if (!state) {
      return {
        ok: false,
        error: "http_timeout",
        launched,
        ...spawnInfo,
        hint: "Wait for Boot/Social, then retry director. Close a hung editor first.",
      };
    }
    const leaveBoot = Date.now() + 45000;
    if (state?.scene === "boot") {
      try {
        await request("POST", "/director", { action: "enter" });
      } catch {
        /* travel still may proceed */
      }
    }
    while (state?.scene === "boot" && Date.now() < leaveBoot) {
      await sleep(500);
      state = (await probeState()) || state;
    }
  }
  if (!activity || activity === "none") {
    return { ok: true, launched, ...spawnInfo, state };
  }
  try {
    if (activity === "composer") {
      await request("POST", "/director", { action: "pvp" });
      state = await waitForScene("composer", 45000);
      return { ok: true, launched, ...spawnInfo, state };
    }
    if (activity === "arena") {
      await request("POST", "/director", { action: "arena" });
      state = await waitForScene("pvp", 60000);
      if (state?.scene === "pvp") state = await waitForNavTiles(45000);
      return { ok: true, launched, ...spawnInfo, state };
    }
    if (activity === "pvp") {
      await request("POST", "/director", { action: "pvp" });
      state = await waitForScene("composer", 45000);
      if (state?.scene === "pvp") {
        state = await waitForNavTiles(45000);
        return { ok: true, launched, ...spawnInfo, state };
      }
      if (state?.scene !== "composer") {
        return {
          ok: false,
          error: `expected composer after pvp, got ${state?.scene}`,
          launched,
          ...spawnInfo,
          state,
        };
      }
      state = await completeComposerIntoMatch();
      return { ok: true, launched, ...spawnInfo, state };
    }
    await request("POST", "/director", { action: activity });
    state = await waitForScene(activity, 45000);
    return { ok: true, launched, ...spawnInfo, state };
  } catch (err) {
    return { ok: false, error: String(err.message || err), launched, ...spawnInfo, state: await probeState() };
  }
}

const tools = [
  {
    name: "state",
    description:
      "Read a Calling pawn and scene. Pass seat to sample that hub seat's driven pawn (GET /state?seat=). Omit seat for the listen-server / last-joined pawn. PIE or -game must be running.",
    inputSchema: {
      type: "object",
      properties: {
        seat: { type: "string", description: "Hub seat id (GUID). Two agents must not share one probe." },
      },
    },
  },
  {
    name: "intent",
    description:
      "No-lobby motor: drive the HTTP singleton pawn and abort its sequence/goto. Loopback only. Do not use when a lobby seat exists — hub appendBotBook. Empty {} releases the stick.",
    inputSchema: {
      type: "object",
      properties: {
        move: {
          type: "object",
          properties: { x: { type: "number" }, y: { type: "number" } },
        },
        look: {
          type: "object",
          properties: {
            yaw: { type: "number" },
            pitch: { type: "number" },
            yawAbs: { type: "number" },
            pitchAbs: { type: "number" },
          },
        },
        sprint: { type: "boolean" },
        crouch: { type: "boolean" },
        ads: { type: "boolean" },
        fire: { type: "boolean" },
        jump: { type: "boolean" },
        dodge: { type: "boolean" },
        dash: { type: "boolean" },
        reload: { type: "boolean" },
        swap: { type: "boolean" },
      },
    },
  },
  {
    name: "hold",
    description:
      "No-lobby: queue one timed hold on the HTTP singleton (30 Hz). Loopback only. Do not use when a lobby seat exists — hub appendBotBook.",
    inputSchema: {
      type: "object",
      properties: {
        seconds: { type: "number" },
        move: {
          type: "object",
          properties: { x: { type: "number" }, y: { type: "number" } },
        },
        look: {
          type: "object",
          properties: {
            yaw: { type: "number" },
            pitch: { type: "number" },
            yawAbs: { type: "number" },
            pitchAbs: { type: "number" },
          },
        },
        sprint: { type: "boolean" },
        crouch: { type: "boolean" },
        ads: { type: "boolean" },
        fire: { type: "boolean" },
        jump: { type: "boolean" },
        dodge: { type: "boolean" },
        dash: { type: "boolean" },
        reload: { type: "boolean" },
        swap: { type: "boolean" },
        replaceFrom: { type: "string", description: "now (default) or afterCurrent" },
      },
      required: ["seconds"],
    },
  },
  {
    name: "sequence",
    description:
      "No-lobby: queue timed steps on the HTTP singleton. Loopback only. Do not use when a lobby seat exists — hub appendBotBook.",
    inputSchema: {
      type: "object",
      properties: {
        replaceFrom: { type: "string", description: "now (default) or afterCurrent" },
        steps: {
          type: "array",
          items: {
            type: "object",
            properties: {
              seconds: { type: "number" },
              move: {
                type: "object",
                properties: { x: { type: "number" }, y: { type: "number" } },
              },
              look: {
                type: "object",
                properties: {
                  yaw: { type: "number" },
                  pitch: { type: "number" },
                  yawAbs: { type: "number" },
                  pitchAbs: { type: "number" },
                },
              },
              sprint: { type: "boolean" },
              crouch: { type: "boolean" },
              ads: { type: "boolean" },
              fire: { type: "boolean" },
              jump: { type: "boolean" },
              dodge: { type: "boolean" },
              dash: { type: "boolean" },
              reload: { type: "boolean" },
              swap: { type: "boolean" },
            },
          },
        },
      },
      required: ["steps"],
    },
  },
  {
    name: "goto",
    description:
      "No-lobby Recast follow on the HTTP singleton pawn. Loopback only. Do not use when a lobby seat exists — hub appendBotBook (JIT puml goto, or catalog marker). Cancelled by intent or sequence.",
    inputSchema: {
      type: "object",
      properties: {
        x: { type: "number" },
        y: { type: "number" },
        z: { type: "number" },
      },
      required: ["x", "y"],
    },
  },
  {
    name: "respawn",
    description:
      "Teleport the local pawn to start (or RestartPlayer if it was destroyed). Use after a void fall instead of relaunching PvP.",
    inputSchema: { type: "object", properties: {} },
  },
  {
    name: "director",
    description:
      "I-menu overlay + composer HUD twins. action: open, close, toggle, director, keybinds, pvp/composer (Compose PvP), host, guest, ready, go/start, arena (solo skip), raid, practice, social. Remote join/ready/go/appendBotBook/mindControl are hub.",
    inputSchema: {
      type: "object",
      properties: {
        action: {
          type: "string",
          description: "open | close | toggle | director | keybinds | pvp | composer | host | guest | ready | go | start | arena | raid | practice | social",
        },
      },
      required: ["action"],
    },
  },
  {
    name: "hub",
    description:
      "Session hub (same codec as ws://127.0.0.1:18766). Anytime you drive a pawn, use appendBotBook / branchBotBook (catalog name or JIT puml). join headless is an anchor only (kind cursor by default); then mindControl. setTeam, ready, go, subscribe, view. Loopback plan/goto only when no seat exists. POST /hub.",
    inputSchema: {
      type: "object",
      properties: {
        type: { type: "string", description: "join | subscribe | ready | go | mindControl | setTeam | appendBotBook | branchBotBook | view | plan | goto" },
        displayName: { type: "string" },
        headless: { type: "boolean" },
        kind: { type: "string", description: "cursor (default on this MCP) | remoteAgent | algorithmic" },
        seatId: { type: "string" },
        targetSeatId: { type: "string" },
        team: { type: "string", description: "red | blue | unassigned" },
        ready: { type: "boolean" },
        botBook: { type: "string", description: "Catalog BotBook name (appendBotBook / branchBotBook)" },
        puml: { type: "string", description: "JIT PlantUML body (restricted subset). xyz goto allowed only here." },
        afterId: { type: "string", description: "branchBotBook: node id to replace from" },
        offset: { type: "number", description: "branchBotBook: remaining-walk offset" },
        x: { type: "number" },
        y: { type: "number" },
        z: { type: "number" },
        replaceFrom: { type: "string", description: "now, afterCurrent, or remainder (loopback plan)" },
        steps: { type: "array", items: { type: "object" } },
      },
      required: ["type"],
    },
  },
  {
    name: "boot",
    description:
      "If 18765 is down, spawn UnrealEditor (standalone -game by default, or editor+PIE). Then director activity: pvp (default) races Compose lobby into the match; composer stops in lobby; arena is solo skip.",
    inputSchema: {
      type: "object",
      properties: {
        mode: {
          type: "string",
          description: "game (standalone, default) or editor (UnrealEditor + PIE python)",
        },
        activity: {
          type: "string",
          description: "pvp (Compose then auto Go into match, default), composer (stop in lobby), arena (solo skip), raid, practice, social, or none",
        },
        waitSeconds: { type: "number", description: "HTTP wait after spawn (default 90)" },
      },
    },
  },
];

async function callTool(name, args) {
  const a = args || {};
  if (name === "state") {
    const q = a.seat ? `?seat=${encodeURIComponent(a.seat)}` : "";
    return request("GET", `/state${q}`);
  }
  if (name === "intent") {
    return request("POST", "/intent", a);
  }
  if (name === "hold") {
    return request("POST", "/sequence", {
      steps: [{ ...a, seconds: a.seconds }],
      replaceFrom: a.replaceFrom,
    });
  }
  if (name === "sequence") {
    return request("POST", "/sequence", a);
  }
  if (name === "goto") {
    return request("POST", "/goto", a);
  }
  if (name === "respawn") {
    return request("POST", "/respawn", a);
  }
  if (name === "director") {
    return request("POST", "/director", { action: a.action || "toggle" });
  }
  if (name === "hub") {
    const body = { ...a };
    if (String(body.type || "").toLowerCase() === "join" && !body.kind) {
      body.kind = "cursor";
    }
    return request("POST", "/hub", body);
  }
  if (name === "boot") {
    return boot(a);
  }
  throw new Error(`unknown tool ${name}`);
}

function reply(id, result, error) {
  const msg = error
    ? { jsonrpc: "2.0", id, error }
    : { jsonrpc: "2.0", id, result };
  process.stdout.write(JSON.stringify(msg) + "\n");
}

const rl = readline.createInterface({ input: process.stdin });
rl.on("line", async (line) => {
  if (!line.trim()) return;
  let msg;
  try {
    msg = JSON.parse(line);
  } catch {
    return;
  }
  const { id, method, params } = msg;
  try {
    if (method === "initialize") {
      reply(id, {
        protocolVersion: "2024-11-05",
        capabilities: { tools: {} },
        serverInfo: { name: "destiny-like-agent", version: "0.3.0" },
      });
      return;
    }
    if (method === "notifications/initialized" || method === "initialized") {
      return;
    }
    if (method === "tools/list") {
      reply(id, { tools });
      return;
    }
    if (method === "tools/call") {
      const name = params?.name;
      const args = params?.arguments || {};
      const data = await callTool(name, args);
      reply(id, {
        content: [{ type: "text", text: JSON.stringify(data, null, 2) }],
      });
      return;
    }
    if (method === "ping") {
      reply(id, {});
      return;
    }
    reply(id, undefined, { code: -32601, message: `Unknown method ${method}` });
  } catch (err) {
    reply(id, undefined, { code: -32000, message: String(err.message || err) });
  }
});
