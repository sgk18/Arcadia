import { execFile } from "child_process";
import path from "path";
import { NextRequest, NextResponse } from "next/server";

// Absolute path to the compiled C binary (one level above the Next.js app).
const EXE_PATH = path.join(process.cwd(), "..", "arcadia.exe");
// The C binary reads/writes game_state.dat relative to its cwd.
const EXE_CWD = path.join(process.cwd(), "..");

/**
 * Execute the C binary with the given argv arguments and return parsed JSON.
 * Rejects on non-zero exit OR if stdout is not valid JSON.
 */
function runGame(args: string[]): Promise<Record<string, unknown>> {
  return new Promise((resolve, reject) => {
    execFile(EXE_PATH, args, { cwd: EXE_CWD, timeout: 5000 }, (err, stdout, stderr) => {
      const raw = stdout?.trim();

      // Even on error, the binary may have written a JSON error object to stdout.
      if (raw) {
        try {
          const json = JSON.parse(raw) as Record<string, unknown>;
          resolve(json);
          return;
        } catch {
          // fall through to reject below
        }
      }

      if (err) {
        reject(new Error(stderr || err.message));
        return;
      }

      reject(new Error("Empty or non-JSON response from game engine."));
    });
  });
}

/**
 * POST /api/game
 *
 * Body (JSON):
 * {
 *   "command": "init" | "combat" | "move" | "buy" | "skill",
 *   "username": "Kael",
 *   "args": []          // optional extra positional args
 * }
 *
 * Examples:
 *   { "command": "init",   "username": "Kael" }
 *   { "command": "combat", "username": "Kael" }
 *   { "command": "move",   "username": "Kael", "args": ["N"] }
 *   { "command": "buy",    "username": "Kael" }
 *   { "command": "skill",  "username": "Kael", "args": ["rotate"] }
 *   { "command": "skill",  "username": "Kael", "args": ["learn", "Blaze Strike", "70"] }
 */
export async function POST(req: NextRequest) {
  let body: { command?: string; username?: string; args?: string[] };

  try {
    body = (await req.json()) as typeof body;
  } catch {
    return NextResponse.json(
      { error: "Invalid JSON body." },
      { status: 400 }
    );
  }

  const { command, username, args = [] } = body;

  if (!command || typeof command !== "string") {
    return NextResponse.json(
      { error: "Missing required field: command." },
      { status: 400 }
    );
  }

  if (!username || typeof username !== "string") {
    return NextResponse.json(
      { error: "Missing required field: username." },
      { status: 400 }
    );
  }

  // Build argv: [command, username, ...args]
  const argv = [command, username, ...args.map(String)];

  try {
    const gameState = await runGame(argv);
    return NextResponse.json(gameState);
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    return NextResponse.json({ error: message }, { status: 500 });
  }
}

/**
 * GET /api/game?username=Kael
 * Convenience endpoint — loads (or creates) a player without fighting.
 */
export async function GET(req: NextRequest) {
  const username = req.nextUrl.searchParams.get("username") ?? "Traveler";

  try {
    const gameState = await runGame(["init", username]);
    return NextResponse.json(gameState);
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    return NextResponse.json({ error: message }, { status: 500 });
  }
}
