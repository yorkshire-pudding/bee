<?php

/**
 * The table Printer class has no dependencies and generates an array of strings
 * out of an array of objects or arrays and the child properties that should be printed given by an array of strings
 *
 * Should result in something like the following:
 * +------+---------------+----------------+---------------+
 * |  id  |     name      | col_name_three | col_name_four |
 * +------+---------------+----------------+---------------+
 * | 1138 | Bill Murray   | thing 1        | thing 2       |
 * | 2020 | Jeff Goldblum | thing 1        | thing 2       |
 * | 117  | John-117      | thing 1        | thing 2       |
 * +------+---------------+----------------+---------------+
 *
 * Credit: https://gist.github.com/redestructa/2a7691e7f3ae69ec5161220c99e2d1b3
 */
class TableBuilder {
    /**
     * Echo the rendered table rows
     *
     * @param array|string[] $renderedTableRows List of rendered table row strings
     */
    public function echoTableRows(array $renderedTableRows): void {
        $eol = \PHP_EOL;
        $green = '[32m'; // TODO: Make this configurable
        $defaultColor = '[39m';

        $i = 0;
        foreach ($renderedTableRows as $rowStr) {
            $isHeaderRow = $i === 1;
            $rowColor = $isHeaderRow ? $green : $defaultColor; // Make the header row green
            echo "\033{$rowColor}{$rowStr}{$eol}";
            $i++;
        }
    }

    /**
     * Get an array of rendered table rows
     *
     * @param array|array[]|object[] $tableData Array of arrays/objects representing the table rows and corresponding data
     * @param array|string[] $headers Array of table header strings
     * @return array|string[] List of rendered table row strings
     */
    public function getTableRows(array $tableData, array $headers): array {
        $lines = [];
        $columnWidths = $this->calculateColumnWidths($tableData, $headers);
        $rowSeparator = $this->renderRowSeparator($columnWidths);

        $lines[] = $rowSeparator; // Add the top border of the table
        if (!empty($headers)) {
            $lines[] = $this->renderHeader($headers, $columnWidths); // Add the rendered table header.
            $lines[] = $rowSeparator; // Add the separator under the header.
        }



        foreach ($tableData as $item) {
            $lines[] = $this->renderRow($item, $columnWidths); // Add rendered data rows
        }

        $lines[] = $rowSeparator; // Add bottom border of the table

        return $lines;
    }

    /**
     * Set the table column widths using the greatest width checking the header and data values
     *
     * @param array|array[]|object[] $tableData Array of table row data (row data may be an array or object)
     * @return array List of column widths uses column name as the key
     */
    public function calculateColumnWidths(array $tableData, array $headers): array {
        $columnWidths = [];

        if (!empty($headers)) {
            foreach ($headers as $column => $header) {
                $columnWidths[$column] = mb_strlen($header, "UTF-8");
            }
        }

        foreach ($tableData as $rowData) {
            foreach ($rowData as $column => $data) {
                $data_width = mb_strlen($data, "UTF-8");
                // Set the column width using the largest length be that from the data or the column header name
                // $columnWidths[$column] = max($columnWidths[$column] ?? grapheme_strlen($column), grapheme_strlen($data));
                // $columnWidths[$column] = max($columnWidths[$column] ?? mb_strlen($column, "UTF-16"), mb_strlen($data, "UTF-16"));
                $columnWidths[$column] = max(isset($columnWidths[$column]) ? $columnWidths[$column] : 0, $data_width);
                // $columnWidths[$column] = max($columnWidths[$column] ?? mb_strwidth($column), mb_strwidth($data));
                // $columnWidths[$column] = max($columnWidths[$column] ?? mb_strwidth($column, "UTF-8"), mb_strwidth($data, "UTF-8"));
                // $columnWidths[$column] = max($columnWidths[$column] ?? mb_strwidth($column, "UTF-16"), mb_strwidth($data, "UTF-16"));

            }
        }
        // Adjust the width of the widest column to fit in the terminal. If a command
        // outputting a table is run in a non-interactive shell (e.g. as part of an
        // SSH command) then 'tput cols' on its own will create an error. This
        // provides an alternative if 'tput cols' create an error.
        $terminal_width = exec('if tput cols &>/dev/null; then  echo $(tput cols); else $(echo $COLUMNS); fi');
        bee_instant_message(bt('The terminal width is: ') . $terminal_width, 'debug');
        $max_potential_width = array_sum($columnWidths);
        bee_instant_message(bt('The max potential width is: ') . $max_potential_width, 'debug');
        $column_count = count($columnWidths);
        bee_instant_message(bt('The column count is: ') . $column_count, 'debug');

        return $columnWidths;
    }

    /**
     * Create horizontal row seperator string
     *
     * @param array $columnWidths List of column widths
     * @param string $rowColumnIntersect Text character to be used at the intersection of rows and columns, defaults to '+'
     * @return string Horizontal row separator
     */
    public function renderRowSeparator(array $columnWidths, string $rowColumnIntersect = '+'): string {
        $spacesAroundData = 2;
        $separatorStr = $rowColumnIntersect;

        foreach ($columnWidths as $width) {
            $separatorStr .= $this->strRepeat('-', $width + $spacesAroundData);
            $separatorStr .= $rowColumnIntersect;
        }

        return $separatorStr;
    }

    /**
     * Append a string to itself a given number of times
     *
     * @param string $str String to be repeated
     * @param int $count The number of times to repeat the string
     * @return string The repeated string
     */
    public function strRepeat(string $str, int $count): string {
        $str2 = '';
        for ($i = $count; $i > 0; $i--) {
            $str2 .= $str;
        }

        return $str2;
    }

    /**
     * Render the table header
     *
     * @param array $headers List of table headers
     * @param array $columnSize List of column widths
     * @return string Rendered table header
     */
    public function renderHeader(array $headers, array $columnSize): string {
        return $this->renderRow($headers, $columnSize, STR_PAD_BOTH);
    }

    /**
     * Create table row strings
     *
     * @param array|object $rowData Array or Object of key value row data
     * @param array $columnSize List of column widths
     * @return string Rendered table row
     */
    public function renderRow($rowData, array $columnSize, int $padType = STR_PAD_RIGHT): string {
        $rowString = '';

        foreach ($rowData as $column => $data) {
            // This accounts for headers not having the corresponding column keys as the data itself is the column key
            $rowWidth = $columnSize[$column] ?? $columnSize[$data];
            $count_emojis = $this->string_emoji_count($data);
            if ($count_emojis > 0) {
                $rowWidth = $rowWidth + ($count_emojis * 2);
            }
            $rowString .= '| ' . str_pad($data, $rowWidth, ' ', $padType) . ' ';

            // Add the outer right side of the table if on last column
            $headers = array_keys($rowData);
            // If two columns have the same data, this prevents incorrectly adding the '|'
            $rowString .= end($headers) === $column ? '|' : '';
        }
        return $rowString;
    }

    /**
     * Add two numeric values
     *
     * @param int $a Value to be added
     * @param int $b Value to be added
     * @return int The summed value of the arguments
     */
    public function sum(?int $a, int $b): int {
        return ($a ?? 0) + $b;
    }

    /**
     * Check for and count emojis in string.
     *
     * @param string $string
     *
     * @return int
     *   Return a count of all emojis in string.
     */
    public function string_emoji_count($string): int {
        $emojis_regex = '/[\x{0080}-\x{02AF}'     // Latin-1 Supplement, IPA Extensions
        .'\x{0300}-\x{03FF}'      // Combining Diacritical Marks, Greek
        .'\x{0600}-\x{06FF}'      // Arabic
        .'\x{0C00}-\x{0C7F}'      // Telugu
        .'\x{1DC0}-\x{1DFF}'      // Combining Diacritical Marks Supplement
        .'\x{1E00}-\x{1EFF}'      // Latin Extended Additional
        .'\x{2000}-\x{209F}'      // General Punctuation
        .'\x{20D0}-\x{214F}'      // Combining Diacritical Marks for Symbols, Letterlike Symbols
        .'\x{2190}-\x{23FF}'      // Arrows, Mathematical Operators
        .'\x{2460}-\x{25FF}'      // Enclosed Alphanumerics, Geometric Shapes
        .'\x{2600}-\x{27EF}'      // Miscellaneous Symbols, Dingbats
        .'\x{2900}-\x{29FF}'      // Supplemental Arrows-B
        .'\x{2B00}-\x{2BFF}'      // Miscellaneous Symbols and Arrows
        .'\x{2C60}-\x{2C7F}'      // Latin Extended-C
        .'\x{2E00}-\x{2E7F}'      // Supplemental Punctuation
        .'\x{3000}-\x{303F}'      // CJK Symbols and Punctuation
        .'\x{A490}-\x{A4CF}'      // Yi Radicals
        .'\x{E000}-\x{F8FF}'      // Private Use Area
        .'\x{FE00}-\x{FE0F}'      // Variation Selectors
        .'\x{FE30}-\x{FE4F}'      // CJK Compatibility Forms
        .'\x{1F000}-\x{1F02F}'    // Mahjong Tiles
        .'\x{1F0A0}-\x{1F0FF}'    // Playing Cards
        .'\x{1F100}-\x{1F64F}'    // Enclosed Alphanumeric Supplement, Emoticons
        .'\x{1F680}-\x{1F6FF}'    // Transport and Map Symbols
        .'\x{1F700}-\x{1F77F}'    // Alchemical Symbols
        .'\x{1F780}-\x{1F7FF}'    // Geometric Shapes Extended
        .'\x{1F800}-\x{1F8FF}'    // Supplemental Arrows-C
        .'\x{1F900}-\x{1F9FF}'    // Supplemental Symbols and Pictographs
        .'\x{1FA00}-\x{1FA6F}'    // Chess Symbols, Symbols and Pictographs Extended-A
        .'\x{1FA70}-\x{1FAFF}'    // Symbols and Pictographs Extended-B
        .'\x{20000}-\x{2FFFF}'    // CJK Unified Ideographs Extension B-C
        .']/u';

        $emoji_count = preg_match_all($emojis_regex, $string, $matches);
        return $emoji_count;
    }
}
