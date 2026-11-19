/**
 * Сборка MCP-сервера.
 *
 * Зачем esbuild, а не только tsc: общая модель `@ssvc/rectification` — локальный
 * пакет (`file:../packages/rectification`). В tarball и Docker-образ он не попадёт,
 * поэтому модель включается в `dist/index.js` на этапе сборки. Внешними остаются
 * только настоящие npm-зависимости рантайма: SDK и zod.
 */

import { build } from 'esbuild';
import { rm } from 'node:fs/promises';

await rm('dist', { recursive: true, force: true });

await build({
  entryPoints: ['src/index.ts'],
  outfile: 'dist/index.js',
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'node22',
  sourcemap: true,
  // SDK и zod остаются в node_modules: их версии фиксирует package-lock.
  external: ['@modelcontextprotocol/sdk', '@modelcontextprotocol/sdk/*', 'zod'],
  logLevel: 'info',
});
