import re
import os
import glob


def sort_key(filename):
    """Sort key: 1, 2, ..., 9, 9p, 10, 11, ..."""
    name = os.path.splitext(filename)[0]
    match = re.match(r'(\d+)(.*)', name)
    if match:
        num = int(match.group(1))
        suffix = match.group(2)
        return (num, suffix)
    return (float('inf'), name)


def extract_comments(filepath):
    """Extract all // and /* */ comments from a C++ file."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    comments = []
    i = 0
    n = len(content)

    while i < n:
        # // single-line comment
        if content[i] == '/' and i + 1 < n and content[i + 1] == '/':
            end = content.find('\n', i)
            if end == -1:
                end = n
            comments.append(content[i:end])
            i = end

        # /* multi-line comment */
        elif content[i] == '/' and i + 1 < n and content[i + 1] == '*':
            end = content.find('*/', i + 2)
            if end == -1:
                end = n
            else:
                end += 2
            comments.append(content[i:end])
            i = end

        # string literal (skip to avoid false matches)
        elif content[i] == '"':
            i += 1
            while i < n and content[i] != '"':
                if content[i] == '\\':
                    i += 1
                i += 1
            i += 1

        # char literal (skip)
        elif content[i] == "'":
            i += 1
            while i < n and content[i] != "'":
                if content[i] == '\\':
                    i += 1
                i += 1
            i += 1

        else:
            i += 1

    return comments


def main():
    cpp_files = glob.glob('*.cpp')
    cpp_files.sort(key=sort_key)

    output_lines = []

    for filename in cpp_files:
        comments = extract_comments(filename)
        if not comments:
            continue

        output_lines.append(f'{"=" * 60}')
        output_lines.append(f'// File: {filename}')
        output_lines.append(f'{"=" * 60}')
        output_lines.append('')

        for comment in comments:
            output_lines.append(comment)
            output_lines.append('')

    output_path = 'merged_comments.txt'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output_lines))

    print(f'Done. {len(cpp_files)} files processed. Output: {output_path}')


if __name__ == '__main__':
    main()
