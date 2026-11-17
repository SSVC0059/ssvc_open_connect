/**
 * Ресурс `openconnect://info` (mcp-server-agents.md §5, mcp-server.md §3.7).
 *
 * Отдаёт то же, что `oc_get_info` + `oc_get_capabilities`: плоскую модель устройства и
 * реестр возможностей вместе с рантайм-флагами. Формат — JSON-текст ресурса.
 *
 * WP11 подключает `registerInfoResource(server, ctx)` к серверу каждой сессии так же,
 * как `registerInfoTools`.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { loadRuntimeFeatures, mapInfo } from '../domain/info.js';
import type { ToolContext } from '../mcp/types.js';

export const INFO_RESOURCE_URI = 'openconnect://info';

export function registerInfoResource(server: McpServer, ctx: ToolContext): void {
  server.registerResource(
    'openconnect-info',
    INFO_RESOURCE_URI,
    {
      title: 'Информация о контроллере OpenConnect',
      description:
        'Версии прошивки и SSVC, совместимость UART API, реестр возможностей и рантайм-флаги.',
      mimeType: 'application/json',
    },
    async (uri) => {
      const info = await ctx.caps.get();
      const runtime = await loadRuntimeFeatures(ctx.client);
      const payload = {
        ...mapInfo(info),
        features: runtime.features,
        features_meta: runtime.meta,
      };
      return {
        contents: [
          {
            uri: uri.href,
            mimeType: 'application/json',
            text: JSON.stringify(payload, null, 2),
          },
        ],
      };
    },
  );
}
