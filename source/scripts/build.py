import os
import sys
import re
import time
import math
import decode
import struct
import subprocess
import convert
from typing import List, Tuple

def parse_header(line: str) -> Tuple[int, str]:
    # Parse a header line and return (level, name).
    # Returns (0, line) if not a header.
    match = re.match(r'^(=+)(.+?)\1\s*$', line)
    if match:
        level = len(match.group(1))
        name = match.group(2).strip()
        return (level, name)
    return (0, line.strip())

def build_hierarchy(lines: List[str]) -> List[List]:
    # Build hierarchical structure from lines.
    result = []
    root = [0]
    result.append(root)
    stack = [(0, 0)] # (level, result_index)
    
    for line in lines:
        line = line.strip()
        if not line:
            continue
            
        level, name = parse_header(line)
        if level > 0:
            # header, pop stack until we find parent level
            while stack and stack[-1][0] >= level:
                stack.pop()
            
            # create new list for this header
            new_list = [0] # count placeholder
            result.append(new_list)
            new_index = len(result) - 1
            
            # add this header to parent's list
            if stack:
                parent_index = stack[-1][1]
                if parent_index >= 0:
                    result[parent_index].append([name, new_index])
                    result[parent_index][0] += 1
            
            # push this level onto stack
            stack.append((level, new_index))

        else:
            # article, add to current header's list
            if stack and stack[-1][1] >= 0:
                parent_index = stack[-1][1]
                result[parent_index].append([name, -1])
                result[parent_index][0] += 1
    
    # sort each list alphabetically by name: headers first (index >= 0), then articles (index == -1)
    for lst in result:
        if len(lst) > 1:
            entries = lst[1:]
            headers = sorted([e for e in entries if e[1] >= 0], key=lambda x: x[0])
            articles = sorted([e for e in entries if e[1] == -1], key=lambda x: x[0])
            lst[1:] = headers + articles
    
    return result

def parse_internal_links(data):
    # Parse internal links from a byte string.
    # Links have the format: !!!!nn!LINKTEXT!!!! where nn is an ASCII integer number and LINKTEXT is arbitrary bytes.
    # Returns tuple: (cleaned_bytes, link_info)
    #   - cleaned_bytes: bytes string with link wrappers removed
    #   - link_info: list of [start_idx, end_idx, value] for each link, where indices are relative to cleaned_bytes
    result = bytearray()
    links = []
    pos = 0

    while pos < len(data):
        # find start of link
        marker_pos = data.find(b'!!!!', pos)
        if marker_pos == -1:
            # no more links
            result.extend(data[pos:])
            break
        result.extend(data[pos:marker_pos])
        header_start = marker_pos + 4
        bang_pos = data.find(b'!', header_start)
        if bang_pos == -1:
            # safety: malformed header, treat as regular data
            result.extend(data[marker_pos:])
            break

        # parse the article number
        try:
            link_value = int(data[header_start:bang_pos].decode('ascii'))
        except ValueError:
            # safety: not a valid integer
            result.extend(data[marker_pos:])
            break

        # find end of link
        link_text_start = bang_pos + 1
        closing_marker = data.find(b'!!!!', link_text_start)
        if closing_marker == -1:
            # safety: no closing marker
            result.extend(data[marker_pos:])
            break

        # rebuild link
        link_text = data[link_text_start:closing_marker]
        start_idx = len(result)
        end_idx = start_idx + len(link_text)
        links.append([start_idx, end_idx, link_value])
        result.extend(link_text)
        pos = closing_marker + 4

    return bytes(result), links

def compress(data: bytes) -> bytes:
    # Compress data using ZX0 (with salvador).
    # Returns the packed data in File_ReadComp() format.
    last_four = data[-4:] if len(data) >= 4 else data + b'\x00' * (4 - len(data))
    data_to_compress = data[:-4] if len(data) >= 4 else b''
    temp_input = "temp.$$$"
    temp_output = "temp2.$$$"
    
    try:
        with open(temp_input, 'wb') as f:
            f.write(data_to_compress)
        subprocess.run(['salvador', temp_input, temp_output], check=True)
        with open(temp_output, 'rb') as f:
            compressed_data = f.read()
        payload = last_four + b'\x00\x00' + compressed_data
        length = len(payload)
        length_bytes = struct.pack('<H', length)  # Little-endian unsigned short
        return length_bytes + payload
        
    finally:
        # clean up temporary files
        if os.path.exists(temp_input):
            os.remove(temp_input)
        if os.path.exists(temp_output):
            os.remove(temp_output)

def print_progress(pct, width=30):
    pct = max(0.0, min(100.0, pct))
    filled = int(width * pct / 100.0)
    bar = []
    for i in range(width):
        if i < filled:
            bar.append("█")
        else:
            bar.append("░")
    sys.stdout.write(f"\r[{''.join(bar)}] {pct:6.2f}%")
    sys.stdout.flush()

    if pct >= 100.0:
        sys.stdout.write("\n")
        
def split_chunks(lst, n):
    q, r = divmod(len(lst), n)
    out = []
    start = 0
    for i in range(n):
        end = start + q + (i < r)
        out.append(lst[start:end])
        start = end
    return out
        
# main code
if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <input_file>", file=sys.stderr)
    sys.exit(1)

filename = sys.argv[1]

try:
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        convert.whole_index = [line.strip("\n") for line in lines]
except FileNotFoundError:
    print(f"Error: File '{filename}' not found", file=sys.stderr)
    sys.exit(1)
except Exception as e:
    print(f"Error reading file: {e}", file=sys.stderr)
    sys.exit(1)

# build index hierarchy and get (decoded) lengths for faster traversal later
index_lists = build_hierarchy(lines)
index_lists_len = []
max_index_len = 0
for item in index_lists:
    addition = [item[0]]
    this_len_sum = 0
    for item2 in item[1:]:
        this_len = len(decode.decode_bytes(item2[0]))
        this_len_sum += this_len
        addition.append(this_len)
    index_lists_len.append(addition)
    if this_len_sum > max_index_len:
        max_index_len = this_len_sum

# create and write index/data
major_categories = {"People": 0, "History": 0, "Geography": 0, \
                    "Arts": 0, "Philosophy and religion": 0, "Everyday life": 0, "Society": 0, \
                    "Life sciences": 0, "Physical sciences": 0, "Technology": 0, "Mathematics": 0}
write_order = []
article_addrs = {}
index_len = 0
data_len = 0
max_category_len = 0
max_category_label = ""
old_data_len = data_len
index_bytes = b""
total_articles = 0
last_call = 0.0
min_interval = 0.25 # no more than API 4 calls per second, to avoid rate limiting by Wikipedia's servers
for items in index_lists:
    index_bytes += items[0].to_bytes(1, "little")
    index_len += 1
    if len(items) > max_category_len:
        max_category_len = len(items)
        max_category_label = items[1][0]
    for item in items[1:]:
        if item[1] == -1:
            # article
            total_articles += 1
            article_count = convert.whole_index.index(item[0])
            write_order.append(article_count)
            article_addrs[article_count] = data_len
            print(str(article_count) + ": " + item[0])
            
            # build index    
            index_bytes += (data_len | 0x80000000).to_bytes(4, "little")
            index_bytes += decode.decode_bytes(item[0])
            index_bytes += chr(0).encode("latin1")
            index_len += (4 + len(decode.decode_bytes(item[0])) + 1)
            
            # do we already have it cached?
            if os.path.isfile("./articles/" + str(article_count) + ".dat"):
                # already built - just update data_len
                data_len += os.path.getsize("./articles/" + str(article_count) + ".dat") - 16
                
            else:
                # need to build article - rate-limit
                elapsed = time.time() - last_call
                if elapsed < min_interval:
                    time.sleep(min_interval - elapsed)
                last_call = time.time()
                
                # build pages
                pages = convert.simplify_wikipedia_page(item[0]) + chr(0).encode("latin1")
                pages = convert.split_page(pages)
                content_bytes = b""
                total_pages = 0
                total_words = 0
                total_bytes = 0
                total_links = 0
                for index, page in enumerate(pages):
                    total_pages += 1
                    page, links = parse_internal_links(page)
                    compressed_page = compress(page)
                    total_words += len(page.split(b" "))
                    total_bytes += len(page)
                    if index > 0:
                        links.append([0, 14, old_data_len | 0x80000000])
                    old_data_len = data_len
                    data_len += (2 + len(compressed_page) + 100 + 2 + len(links)*8)
                    if len(pages) > 1 and index < len(pages) - 1:
                        data_len += 8
                        links.append([len(page) - 12, len(page), data_len | 0x80000000])
                    total_links += len(links)
                    content_bytes += len(page).to_bytes(2, "little")
                    content_bytes += compressed_page
                    content_bytes += decode.decode_bytes(item[0]).ljust(100, b'\x00')
                    content_bytes += len(links).to_bytes(2, "little")
                    content_bytes += b''.join([link[0].to_bytes(2, "little") + link[1].to_bytes(2, "little") + link[2].to_bytes(4, "little") for link in links])
                with open("./articles/" + str(article_count) + ".dat", "wb") as fa:
                    fa.write(total_pages.to_bytes(4, "little"))
                    fa.write(total_words.to_bytes(4, "little"))
                    fa.write(total_bytes.to_bytes(4, "little"))
                    fa.write(total_links.to_bytes(4, "little"))
                    fa.write(content_bytes)
                
        else:
            # index header
            print(item[0].upper())
            index_loc = 0
            for i in range(item[1]):
                index_loc += 1
                for item2 in index_lists_len[i][1:]:
                    index_loc += 5
                    index_loc += item2
            if item[0] in major_categories.keys():
                major_categories[item[0]] = index_loc
            index_bytes += index_loc.to_bytes(4, "little")
            index_bytes += decode.to_bold(decode.decode_bytes(item[0]))
            index_bytes += chr(0).encode("latin1")
            index_len += (4 + len(decode.decode_bytes(item[0])) + 1)
                    
# build combined file
print("________\r\n\r\nBuilding combined files...")
chunks = []
total_pages = 0
total_words = 0
total_bytes = 0
total_links = 0
total_size = 0
for i, article in enumerate(write_order):
    print_progress(i / len(write_order) * 100)
    with open("./articles/" + str(article) + ".dat", "rb") as fa:
        header = fa.read(16)
        total_pages += struct.unpack("<I", header[0:4])[0]
        total_words += struct.unpack("<I", header[4:8])[0]
        total_bytes += struct.unpack("<I", header[8:12])[0]
        total_links += struct.unpack("<I", header[12:16])[0]
        chunks.append(fa.read())
        total_size += len(chunks[-1])
        
# add "about" text
print("\r\nBuilding 'about' text")
reading_time = int(total_words / 300 / 60)
megabytes = int(total_size / 1024 / 1024)
about_text  = b"Encontra '26 is a digital encyclopedia for SymbOS. Article text is adapted from Wikipedia, the Free Encyclopedia, as of January 2026, without modification except for automated page reduction and formatting.\r\n\r\n"
about_text += b"The list of included articles is based on the English Wikipedia's Level 5 Vital Articles list, a community-ranked directory of the 50,000 most important articles on the site. For the benefit of Encontra's expected audience, this list was then expanded by adding several hundred additional articles relating to computer history and specific vintage hardware and software.\r\n\r\n"
about_text += convert.to_bold(b"STATISTICS") + b"\r\n\r\n"
about_text += f"{total_articles:,}".encode("ascii") + b" total articles\r\n"
about_text += f"{total_pages:,}".encode("ascii") + b" total subpages\r\n"
about_text += f"{total_words:,}".encode("ascii") + b" total words\r\n"
about_text += f"{total_bytes:,}".encode("ascii") + b" total characters\r\n"
about_text += f"{total_links:,}".encode("ascii") + b" internal links\r\n\r\n"
about_text += b"At a typical pace of 300 words per minute, reading all the content in Encontra '26 would take about " + f"{reading_time:,}".encode("ascii") + b" hours. (Reading all 5 billion words of the complete English Wikipedia would take about 32 years.) Weighing in at " + str(megabytes).encode("ascii") + b" MB of compressed data, Encontra '26 is believed to be the largest single software package ever shipped for a classic 8-bit microcomputer.\r\n\r\n"
about_text += convert.to_bold(b"SYSTEM REQUIREMENTS") + b"\r\n\r\n"
about_text += convert.to_underline(b"Operating system") + b": SymbOS 4.0 or newer\r\n"
about_text += convert.to_underline(b"Memory") + b": at least 256 KB of RAM (320 KB recommended)\r\n"
about_text += convert.to_underline(b"Storage") + b": mass storage drive with at least " + str(megabytes + 1).encode("ascii") + b" MB of free space\r\n"
about_text += convert.to_underline(b"Input device") + b": mouse\r\n\r\n"
about_text += convert.to_bold(b"DISCLAIMERS") + b"\r\n\r\n"
about_text += b"While considerable effort has gone into rendering pages in a readable way, pages may occasionally reference content (such as images) that are no longer visible or show misleading plain-text renderings of mathematical equations or non-Latin glyphs. When in doubt, the full multimedia page content may be viewed on the English Wikipedia (" + convert.to_underline(b"en.wikipedia.org") + b").\r\n\r\n"
about_text += b"Encontra '26 is provided for informational purposes only. Its creators and distributors are not liable for any actions taken, or damages suffered, as a consequence of the information contained in this software. To correct any factual errors, please see the original pages on the English Wikipedia (" + convert.to_underline(b"en.wikipedia.org") + b").\r\n\r\n"
about_text += b"\"Encontra\" is a form of the Spanish verb \"to find\" and should not be interpreted as implying affiliation with, or endorsement by, the Microsoft Corporation or the former Microsoft Encarta brand.\r\n\r\n"
about_text += convert.to_bold(b"COPYRIGHT") + b"\r\n\r\n"
about_text += b"The Encontra '26 application, source code, art, fonts, and associated files are copyright 2026 by Daniel E. Gaskell under the MIT License. Encontra '26 may be used and distributed without charge.\r\n\r\n"
about_text += b"Text from Wikipedia is freely distributable under the terms of the Creative Commons Attribution-ShareAlike 4.0 International License (CC-BY-SA) and, unless otherwise noted, the GNU Free Documentation License (GFDL). See " + convert.to_underline(b"en.wikipedia.org/wiki/Wikipedia:Copyrights") + b" for further details.\r\n\r\n"
about_text += b"The interface graphics incorporate element of copyrighted photographs from Wikimedia Commons: Ramses_II_British_Museum.jpg (Pbuergler), 2023 Jetsun Pema (cropped)01.jpg (UK Foreign Office), weic2320b (ESS/Webb), and Lateral Plasma Ball.jpg (Stbuccia), licensed under the Creative Commons Attribution 4.0 International License (CC-BY). The remaining graphics are original or modifications of public-domain material.\r\n\x00"
compressed_page = compress(about_text)
about_bytes = len(about_text).to_bytes(2, "little")
about_bytes += compressed_page
about_bytes += decode.decode_bytes("Encontra '26").ljust(100, b'\x00')
about_bytes += b"\x00\x00" # no links
chunks.append(about_bytes)

# convert internal links
print("Updating internal links...")
chunks_final = []
for i, chunk in enumerate(chunks):
    content = bytearray(chunk)
    pos = 0
    while pos < len(chunk):
        # skip past header
        pos += 2
        pos += struct.unpack('<H', content[pos:pos+2])[0] + 102
        print(str(pos) + ":", content[pos-100:pos].split(b'\x00', 1)[0].decode('ascii'))
        
        # process links
        if pos + 2 > len(content):
            break # safety: not enough bytes for link count
        num_links = struct.unpack('<H', content[pos:pos+2])[0]
        pos += 2
        for link in range(num_links):
            if pos + 8 > len(content):
                break # safety: not enough bytes for complete link
            addr = struct.unpack('<I', content[pos+4:pos+8])[0]
            if addr < 0x8000000:
                if addr in article_addrs:
                    new_addr = article_addrs[addr]
                    content[pos+4:pos+8] = struct.pack('<I', new_addr)
                else:
                    print("link malformed at " + str(pos) + " - article " + str(addr))
            else:
                new_addr = addr & 0x7FFFFFFF
                content[pos+4:pos+8] = struct.pack('<I', new_addr)
            pos += 8 # move to next link
        
    # add to list
    chunks_final.append(bytes(content))

# write files
print("Writing files...")
with open("CONTENTS.DB", 'wb') as fi:
    fi.write(index_bytes)
chunks_split = split_chunks(chunks_final, 16)
chunks_start = []
running_len = 0
for i, these_chunks in enumerate(chunks_split):
    out_name = "ARTICLE" + chr(65 + i) + ".DB"
    print(out_name)
    with open(out_name, 'wb') as fd:
        content_bytes = b"".join(these_chunks)
        fd.write(content_bytes)
        chunks_start.append(running_len)
        running_len += len(content_bytes)
    
# final report
print("\nDone. Category index offsets:\n")
print("unsigned long about_addr = " + str(data_len) + ";")
print("unsigned long hover_dest[3][5] =")
print("   {{" + str(major_categories["People"]) + ", " + str(major_categories["Geography"]) + ", " + str(major_categories["History"]) + ", 0, 0},")
print("    {" + str(major_categories["Arts"]) + ", " + str(major_categories["Everyday life"]) + ", " + str(major_categories["Philosophy and religion"]) + ", " + str(major_categories["Society"]) + ", 0},")
print("    {" + str(major_categories["Life sciences"]) + ", " + str(major_categories["Physical sciences"]) + ", " + str(major_categories["Technology"]) + ", " + str(major_categories["Mathematics"]) + ", 0}};")
print("unsigned long chunks_start[16] = {" + ", ".join([str(i) for i in chunks_start]) + "};")
print("\nStatistics:\n")
print(" - Total articles: " + str(total_articles))
print(" - Total pages: " + str(total_pages))
print(" - Total words: " + str(total_words))
print(" - Total characters: " + str(total_bytes))
print(" - Total links: " + str(total_links))
print(" - Most articles in a category: " + str(max_category_len) + " (starts with " + max_category_label + ")")
print(" - Most bytes in an index list: " + str(max_index_len))
