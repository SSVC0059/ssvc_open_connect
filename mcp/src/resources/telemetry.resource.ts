/**
 * Ресурс `openconnect://telemetry` (mcp-server-agents.md §5).
 *
 * Отдаёт последний снимок состояния из `GET /rest/oc/snapshot` вместе с `stale` и
 * `partial[]`. Время в ответе — millis() устройства (uptime), не epoch.
 *
 * WP11 подключает `registerTelemetryResource(server, ctx)` к серверу каждой сессии.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { buildStateView, fetchSnapshot, stalenessTrackerFor } from '../domain/telemetry.js';
import type { ToolContext } from '../mcp/types.js';

export const TELEMETRY_RESOURCE_URI = 'openconnect://telemetry';

export function registerTelemetryResource(server: McpServer, ctx: ToolContext): void {
  server.registerResource(
    'openconnect-telemetry',
    TELEMETRY_RESOURCE_URI,
    {
      title: 'Телеметрия контроллера OpenConnect',
      description:
        'Снимок состояния: телеметрия SSVC, стадия/статус, датчики, сбои, partial[] и stale.',
      mimeType: 'application/json',
    },
    async (uri) => {
      const snapshot = await fetchSnapshot(ctx.client);
      const state = buildStateView(snapshot, stalenessTrackerFor(ctx.client));
      const payload = {
        observed_at: state.observed_at,
        last_update_ms: state.last_update_ms,
        stale: state.stale,
        freshness_age_ms: state.freshness_age_ms,
        telemetry: state.telemetry,
        status: state.status,
        sensor: state.sensor,
        faults: state.faults,
        partial: state.partial,
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
