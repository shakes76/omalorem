.pragma library

// Omalorem editor math (docs/SPEC.md §4.2, §5.6): the edits behind Ctrl+M and
// Ctrl+Shift+M, as pure functions of the text and selection so they can be
// tested without an editor. Each returns the range to replace, the
// replacement, and the selection to leave inside it (offsets into the
// replacement), in the shape EditorMutations.replaceRange takes.

// Ctrl+M: wrap the selection in $…$, or insert $$ with the caret between.
// Whitespace at either end of the selection stays outside the dollars,
// because "$ x $" is not math to the preview.
function inlineMath(text, selectionStart, selectionEnd) {
    var start = Math.min(selectionStart, selectionEnd);
    var end = Math.max(selectionStart, selectionEnd);
    while (start < end && /\s/.test(text.charAt(start)))
        start++;
    while (end > start && /\s/.test(text.charAt(end - 1)))
        end--;
    var selected = text.slice(start, end);
    return {
        start: start,
        end: end,
        replacement: "$" + selected + "$",
        selectionStart: 1,
        selectionEnd: 1 + selected.length
    };
}

// Ctrl+Shift+M: a display block, $$ on its own line above and below the
// selection (or an empty line for the caret). A $$ block can't interrupt a
// paragraph in the preview's parser, so the block gets a blank line on each
// side unless one is already there.
function displayMath(text, selectionStart, selectionEnd) {
    var start = Math.min(selectionStart, selectionEnd);
    var end = Math.max(selectionStart, selectionEnd);
    var selected = text.slice(start, end).replace(/^\n+|\n+$/g, "");
    // Spaces between the block and the text it splits would be left
    // dangling at the end or start of a line.
    while (start > 0 && /[ \t]/.test(text.charAt(start - 1)))
        start--;
    while (end < text.length && /[ \t]/.test(text.charAt(end)))
        end++;

    var before = "";
    // lastIndexOf clamps a negative index to 0, which would find a newline
    // at 0 for a caret at 0; there the line starts at 0 anyway.
    var lineStart = start > 0 ? text.lastIndexOf("\n", start - 1) + 1 : 0;
    if (text.slice(lineStart, start).trim().length > 0) {
        before = "\n\n";
    } else if (lineStart > 0) {
        var previousStart = text.lastIndexOf("\n", lineStart - 2) + 1;
        if (text.slice(previousStart, lineStart - 1).trim().length > 0)
            before = "\n";
        // The rest of this line is only whitespace; replace it.
    }
    if (text.slice(lineStart, start).trim().length === 0)
        start = lineStart;

    var after = "";
    var lineEnd = text.indexOf("\n", end);
    if (lineEnd < 0)
        lineEnd = text.length;
    if (text.slice(end, lineEnd).trim().length > 0) {
        after = "\n\n";
    } else {
        end = lineEnd;
        if (lineEnd < text.length) {
            var nextEnd = text.indexOf("\n", lineEnd + 1);
            if (nextEnd < 0)
                nextEnd = text.length;
            if (text.slice(lineEnd + 1, nextEnd).trim().length > 0)
                after = "\n";
        }
    }

    var opening = before + "$$\n";
    return {
        start: start,
        end: end,
        replacement: opening + selected + "\n$$" + after,
        selectionStart: opening.length,
        selectionEnd: opening.length + selected.length
    };
}
