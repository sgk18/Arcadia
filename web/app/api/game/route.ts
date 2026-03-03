import { NextRequest, NextResponse } from "next/server";
import {
  cmdInit, cmdCombat, cmdMove, cmdBuy, cmdSkillRotate, cmdSkillLearn,
  EngineState, GameOutput,
} from "@/lib/engine";

/**
 * POST /api/game
 *
 * Body (JSON):
 * {
 *   "command":  "init" | "combat" | "move" | "buy" | "skill",
 *   "username": "Kael",
 *   "args":     [],
 *   "state":    { ...EngineState }   // required for all commands except "init"
 * }
 *
 * The server is stateless — the caller (browser localStorage) holds the
 * authoritative EngineState and sends it with every request.
 */
export async function POST(req: NextRequest) {
  let body: {
    command?: string;
    username?: string;
    args?: string[];
    state?: EngineState;
  };

  try {
    body = (await req.json()) as typeof body;
  } catch {
    return NextResponse.json({ error: "Invalid JSON body." }, { status: 400 });
  }

  const { command, username, args = [], state } = body;

  if (!command) {
    return NextResponse.json({ error: "Missing 'command' field." }, { status: 400 });
  }

  let result: GameOutput;

  try {
    if (command === "init") {
      result = cmdInit(username ?? "Traveler");

    } else {
      if (!state) {
        return NextResponse.json(
          { error: "Missing 'state' field. Run 'init' first." },
          { status: 400 }
        );
      }

      switch (command) {
        case "combat":
          result = cmdCombat(state);
          break;
        case "move":
          result = cmdMove(state, args[0] ?? "");
          break;
        case "buy":
          result = cmdBuy(state);
          break;
        case "skill": {
          const action = args[0] ?? "";
          if (action === "rotate") {
            result = cmdSkillRotate(state);
          } else if (action === "learn") {
            const skillName  = args[1] ?? "";
            const skillPower = parseInt(args[2] ?? "10", 10);
            result = cmdSkillLearn(state, skillName, skillPower);
          } else {
            return NextResponse.json(
              { error: "Unknown skill action. Use: rotate | learn <name> <power>." },
              { status: 400 }
            );
          }
          break;
        }
        default:
          return NextResponse.json(
            { error: `Unknown command: ${command}` },
            { status: 400 }
          );
      }
    }
  } catch (e) {
    return NextResponse.json({ error: String(e) }, { status: 500 });
  }

  return NextResponse.json(result);
}

/** GET /api/game?username=X  →  init a fresh game */
export async function GET(req: NextRequest) {
  const username = req.nextUrl.searchParams.get("username") ?? "Traveler";
  const result = cmdInit(username);
  return NextResponse.json(result);
}
