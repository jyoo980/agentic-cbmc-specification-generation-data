/* No-op stub for editorUpdateSyntax, used only when proving editorUpdateRow.
 *
 * The real editorUpdateSyntax is recursive (it may call itself on the
 * E.syntax!=NULL path), which crashes CBMC's contract inliner when
 * editorUpdateRow's contract is enforced.  editorUpdateSyntax runs only as the
 * final statement of editorUpdateRow, after all of the render/allocation logic
 * (where every mutant lives) has already executed and after row->rsize and
 * row->render have been finalized.  Its effect (recomputing row->hl) is therefore
 * irrelevant to editorUpdateRow's contract, so we replace it with a body-less
 * no-op: this both breaks the recursion and lets the call return normally so the
 * postcondition is reached.  row is left as an incomplete type since the stub
 * never dereferences it. */
typedef struct erow erow;

void editorUpdateSyntax(erow *row)
{
    (void)row;
}
