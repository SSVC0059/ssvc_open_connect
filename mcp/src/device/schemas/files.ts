/**
 * Схемы `/rest/files*` (WP9, mcp-server-agents.md §13; контракт прошивки —
 * mcp-contract-notes.md §B.15, §D.7).
 *
 * Формы, которые нельзя путать:
 *  - `GET /rest/files` — **массив** дерева, а не объект: у каталога есть `children`,
 *    у файла — `size` (`FileHandler::buildFileTree`);
 *  - имя в дереве — только базовое (`file.name()` обрезается по последнему `/`),
 *    поэтому полный путь собирается рекурсивно на стороне сервера;
 *  - `POST /rest/files/copy` отвечает `201` **без тела**, `DELETE` — `204` без тела.
 *
 * `type` описан перечислением `file|directory`: если прошивка начнёт отдавать другие
 * значения, лучше явный `device_error`, чем дерево с молча потерянными узлами.
 */

import { z } from 'zod';

export interface FileTreeEntry {
  name: string;
  type: 'file' | 'directory';
  size?: number;
  children?: FileTreeEntry[];
}

/**
 * Рекурсивный узел дерева файлов.
 *
 * `z.lazy` нужен для самоссылки `children`; тип задан явно, иначе TypeScript
 * выводит бесконечно рекурсивный тип и не собирает проект.
 */
export const fileTreeEntrySchema: z.ZodType<FileTreeEntry> = z.lazy(() =>
  z.object({
    name: z.string(),
    type: z.enum(['file', 'directory']),
    size: z.number().optional(),
    children: z.array(fileTreeEntrySchema).optional(),
  }),
);

/** `GET /rest/files` — массив узлов верхнего уровня. */
export const fileTreeSchema = z.array(fileTreeEntrySchema);

export type FileTree = z.infer<typeof fileTreeSchema>;
