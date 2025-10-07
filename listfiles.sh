#!/bin/bash

# Output file
OUTPUT="listfiles.txt"

# Create or clear the output file
echo "=== TowelHost Project Files ===" > $OUTPUT
echo "Date: $(date)" >> $OUTPUT
echo "=========================" >> $OUTPUT

# List .jucer file
echo -e "\n\n=== TowelHost.jucer ===" >> $OUTPUT
echo "Lines: $(wc -l < "TowelHost.jucer")" >> $OUTPUT
echo "=========================" >> $OUTPUT
cat "TowelHost.jucer" >> $OUTPUT
echo -e "\n\n" >> $OUTPUT

# List CMakeLists.txt
if [[ -f "CMakeLists.txt" ]]; then
    echo -e "\n\n=== CMakeLists.txt ===" >> $OUTPUT
    echo "Lines: $(wc -l < "CMakeLists.txt")" >> $OUTPUT
    echo "=========================" >> $OUTPUT
    cat "CMakeLists.txt" >> $OUTPUT
    echo -e "\n\n" >> $OUTPUT
fi

# List all .h and .cpp files in Source/ root
for file in Source/*.h Source/*.cpp; do
    if [[ -f "$file" ]]; then
        filename=$(basename "$file")
        echo -e "\n\n=== Source/$filename ===" >> $OUTPUT
        echo "Lines: $(wc -l < "$file")" >> $OUTPUT
        echo "=========================" >> $OUTPUT
        cat "$file" >> $OUTPUT
        echo -e "\n\n" >> $OUTPUT
    fi
done

# List all .h and .cpp files in Source/PluginHost/
for file in Source/PluginHost/*.h Source/PluginHost/*.cpp; do
    if [[ -f "$file" ]]; then
        filename=$(basename "$file")
        echo -e "\n\n=== Source/PluginHost/$filename ===" >> $OUTPUT
        echo "Lines: $(wc -l < "$file")" >> $OUTPUT
        echo "=========================" >> $OUTPUT
        cat "$file" >> $OUTPUT
        echo -e "\n\n" >> $OUTPUT
    fi
done

# List all .md files
for file in *.md; do
    if [[ -f "$file" ]]; then
        echo -e "\n\n=== $file ===" >> $OUTPUT
        echo "Lines: $(wc -l < "$file")" >> $OUTPUT
        echo "=========================" >> $OUTPUT
        cat "$file" >> $OUTPUT
        echo -e "\n\n" >> $OUTPUT
    fi
done

echo "Done! Output written to $OUTPUT"
echo "Total files processed:"
echo "  - 1 .jucer file"
[[ -f "CMakeLists.txt" ]] && echo "  - 1 CMakeLists.txt"
echo "  - $(ls Source/*.h Source/*.cpp Source/PluginHost/*.h Source/PluginHost/*.cpp 2>/dev/null | wc -l) source files"
echo "  - $(ls *.md 2>/dev/null | wc -l) markdown files"
