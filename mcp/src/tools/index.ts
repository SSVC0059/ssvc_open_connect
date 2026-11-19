/**
 * Единый каталог сервера: инструменты всех доменов + ресурсы и промпты (WP11, §15).
 *
 * Зачем один файл: до сборки каждый домен держал свои `*Tools()` и подключался
 * отдельной строкой в `src/index.ts`. Двенадцать spread-подключений означали, что
 * «забыл строку» = инструмент не виден клиенту, а два одинаковых имени молча
 * перекрывали друг друга (SDK кладёт инструменты в Map по имени). Теперь источник
 * каталога один, а сборка проверяет инварианты на старте:
 *
 *  - имя инструмента уникально: дубликат — исключение при импорте модуля, а не
 *    тихая потеря одного из инструментов;
 *  - имя соответствует `oc_<домен>_<действие>` (проверяет `defineTool`);
 *  - `title`/`description`/`inputSchema` на месте (тоже `defineTool`).
 *
 * `registerSessionCatalog` — единственная точка подключения того, что живёт «на
 * сессию»: промптов-сценариев и ресурсов `openconnect://…`. HTTP-слой
 * (`src/http/app.ts`) создаёт `McpServer` на каждую сессию и вызывает её ровно один
 * раз, поэтому двойной регистрации быть не может. Инструменты регистрирует
 * `registerTools` из `mcp/registry.ts` — тот же массив `allTools` приходит туда
 * через `createMcpServer({ tools })`.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import type { ToolContext, ToolDef } from '../mcp/types.js';
import { registerPrompts } from '../prompts/prompts.js';
import { registerInfoResource } from '../resources/info.resource.js';
import { registerTelemetryResource } from '../resources/telemetry.resource.js';
import { calcTools } from './calc.tools.js';
import { commandsTools } from './commands.tools.js';
import { filesTools } from './files.tools.js';
import { hardwareTools } from './hardware.tools.js';
import { infoTools } from './info.tools.js';
import { logsTools } from './logs.tools.js';
import { profilesTools } from './profiles.tools.js';
import { relayTools } from './relay.tools.js';
import { sensorsTools } from './sensors.tools.js';
import { settingsTools } from './settings.tools.js';
import { systemTools } from './system.tools.js';
import { telemetryTools } from './telemetry.tools.js';
import { xkc25Tools } from './xkc25.tools.js';

/** Группы по доменам: по одной на файл `src/tools/<домен>.tools.ts`. */
export const toolGroups: ReadonlyArray<{ domain: string; tools: () => ToolDef[] }> = [
  { domain: 'calc', tools: calcTools },
  { domain: 'info', tools: infoTools },
  { domain: 'telemetry', tools: telemetryTools },
  { domain: 'commands', tools: commandsTools },
  { domain: 'settings', tools: settingsTools },
  { domain: 'sensors', tools: sensorsTools },
  { domain: 'relay', tools: relayTools },
  { domain: 'hardware', tools: hardwareTools },
  { domain: 'xkc25', tools: xkc25Tools },
  { domain: 'logs', tools: logsTools },
  { domain: 'profiles', tools: profilesTools },
  { domain: 'files', tools: filesTools },
  { domain: 'system', tools: systemTools },
];

/**
 * Проверяет уникальность имён и возвращает тот же массив.
 *
 * Бросает исключение с перечнем повторов: это дефект сборки каталога, и узнать о нём
 * нужно при старте процесса, а не по «странному» поведению одного из инструментов.
 */
export function assertUniqueToolNames(tools: readonly ToolDef[]): readonly ToolDef[] {
  const seen = new Map<string, number>();
  const duplicates = new Set<string>();
  for (const tool of tools) {
    const count = (seen.get(tool.name) ?? 0) + 1;
    seen.set(tool.name, count);
    if (count > 1) duplicates.add(tool.name);
  }
  if (duplicates.size > 0) {
    throw new Error(
      `Каталог инструментов содержит повторяющиеся имена: ${[...duplicates].sort().join(', ')}. ` +
        'У каждого инструмента должно быть ровно одно определение: дубликат молча перекрыл бы ' +
        'соседний инструмент в реестре MCP.',
    );
  }
  return tools;
}

/** Все инструменты сервиса, собранные из доменов. */
export const allTools: readonly ToolDef[] = assertUniqueToolNames(
  toolGroups.flatMap((group) => group.tools()),
);

/** Имена инструментов в алфавитном порядке — стабильны для тестов и документации. */
export const uniqueToolNames: readonly string[] = Object.freeze(
  allTools.map((tool) => tool.name).sort(),
);

/** Инструмент по имени (для тестов и диагностики); `undefined`, если такого нет. */
export function findTool(name: string): ToolDef | undefined {
  return allTools.find((tool) => tool.name === name);
}

/**
 * Подключает к серверу сессии всё, что не является инструментом: промпты-сценарии
 * (WP10) и ресурсы `openconnect://info` / `openconnect://telemetry` (WP1).
 *
 * Инструменты сюда не входят: ими занимается `registerTools` (`mcp/registry.ts`),
 * иначе они регистрировались бы дважды — SDK отверг бы повторную регистрацию.
 */
export function registerSessionCatalog(server: McpServer, ctx: ToolContext): void {
  registerPrompts(server);
  registerInfoResource(server, ctx);
  registerTelemetryResource(server, ctx);
}
