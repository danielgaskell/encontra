import re
import requests
from decode import decode_bytes
from decode import to_bold
from decode import to_underline
from bs4 import BeautifulSoup
from unidecode import unidecode
from typing import Dict, List, Tuple, Set, Optional

whole_index = []

# Convert equations in LaTeX format to a reasonable ASCII approximation (similar to ASCIImath).
def latex_to_ascii(latex_str):    
    def skip_spaces(s, i):
        # skip whitespace characters starting from index i
        while i < len(s) and s[i].isspace():
            i += 1
        return i
    
    def find_command_name(s, i):
        # extract command name starting at index i (after backslash)
        start = i
        while i < len(s) and s[i].isalpha():
            i += 1
        return s[start:i], i
    
    def parse_braces(s, i):
        # parse a {...} group starting at index i (at the opening brace), handling nested braces
        # returns (content, new_index) where content is inside braces
        if i >= len(s) or s[i] != '{':
            return "", i
        
        depth = 1
        start = i + 1
        i += 1
        
        while i < len(s) and depth > 0:
            if s[i] == '{':
                depth += 1
            elif s[i] == '}':
                depth -= 1
            elif s[i] == '\\':
                # skip escaped characters
                i += 1
                if i < len(s):
                    i += 1
                    continue
            i += 1
        
        # if we exited due to end of string, i is len(s)
        # if due to closing brace, i points after the closing brace
        content = s[start:i-1] if depth == 0 else s[start:]
        return content, i
    
    def parse_brackets(s, i):
        # parse an optional [...] group starting at index i
        # returns (content, new_index) or (None, i) if no bracket
        i = skip_spaces(s, i)
        if i < len(s) and s[i] == '[':
            depth = 1
            start = i + 1
            i += 1
            
            while i < len(s) and depth > 0:
                if s[i] == '[':
                    depth += 1
                elif s[i] == ']':
                    depth -= 1
                elif s[i] == '\\':
                    # skip escaped characters
                    i += 1
                    if i < len(s):
                        i += 1
                        continue
                i += 1
            
            content = s[start:i-1] if depth == 0 else s[start:]
            return content, i
        return None, i
    
    def process_string(s):
        # recursively process the string to handle nested commands
        result = []
        i = 0
        
        while i < len(s):
            if s[i] == '\\':
                # found a command
                cmd_start = i
                i += 1  # skip backslash
                cmd_name, i = find_command_name(s, i)
                
                # check if this is a command we handle
                if cmd_name in ['frac', 'dfrac', 'tfrac']:
                    # parse numerator/denominator
                    i = skip_spaces(s, i)
                    num, i = parse_braces(s, i)
                    i = skip_spaces(s, i)
                    den, i = parse_braces(s, i)
                    
                    # recursively process numerator and denominator
                    num_processed = process_string(num)
                    den_processed = process_string(den)
                    
                    result.append(f"({num_processed})/({den_processed})")
                    
                elif cmd_name in ['binom', 'dbinom', 'tbinom']:
                    # parse top and bottom
                    i = skip_spaces(s, i)
                    top, i = parse_braces(s, i)
                    i = skip_spaces(s, i)
                    bottom, i = parse_braces(s, i)
                    
                    top_processed = process_string(top)
                    bottom_processed = process_string(bottom)
                    
                    result.append(f"C({top_processed},{bottom_processed})")
                    
                elif cmd_name == 'sqrt':
                    # parse optional exponent and argument
                    i = skip_spaces(s, i)
                    exp, i = parse_brackets(s, i)
                    i = skip_spaces(s, i)
                    arg, i = parse_braces(s, i)
                    
                    arg_processed = process_string(arg)
                    
                    if exp:
                        exp_processed = process_string(exp)
                        result.append(f"root[{exp_processed}]({arg_processed})")
                    else:
                        result.append(f"sqrt({arg_processed})")
                        
                else:
                    # command we don't convert - keep it as is
                    result.append(s[cmd_start:i])
                    
            else:
                # regular character
                result.append(s[i])
                i += 1
        
        return ''.join(result)
    
    # convert some operators
    latex_str = latex_str.replace("\\leq", " <= ")
    latex_str = latex_str.replace("\\geq", " >= ")
    latex_str = latex_str.replace("\\ll", " << ")
    latex_str = latex_str.replace("\\gg", " >> ")
    latex_str = latex_str.replace("\\subset", " is a subset of ")
    latex_str = latex_str.replace("\\subseteq", " is a subset of or equal to ")
    latex_str = latex_str.replace("\\in", " is in ")
    latex_str = latex_str.replace("\\supset", " is a superset of ")
    latex_str = latex_str.replace("\\supseteq", " is a superset of or equal to ")
    latex_str = latex_str.replace("\\ni", " is not in ")
    latex_str = latex_str.replace("\\equiv", " is equivalent to ")
    latex_str = latex_str.replace("\\sim", " ~ ")
    latex_str = latex_str.replace("\\simeq", " ~= ")
    latex_str = latex_str.replace("\\approx", " ~= ")
    latex_str = latex_str.replace("\\neq", " != ")
    latex_str = latex_str.replace("\\propto", " is proportional to ")

    # ensure operators have space before/after...
    for op in ['==', '!=', '<=', '>=', '+', '*', '/', '=', '<', '>']:
        latex_str = re.sub(rf'(?<!\s){re.escape(op)}', f' {op}', latex_str) # space before
        latex_str = re.sub(rf'{re.escape(op)}(?!\s)', f'{op} ', latex_str) # space after
    
    # ...but then remove extraneous whitespace
    input_str = ' '.join(latex_str.split())
    result = process_string(input_str)
    
    # convert remaining operators
    result = result.replace("\\cdot", "•")
    result = result.replace("\\dots", " ... ")
    result = result.replace("\\ldots", " ... ")
    result = result.replace("\\vdots", " ... ")
    result = result.replace("\\ddots", " ... ")
    result = result.replace("\\text", "")
    result = result.replace("\\mathbf", "")
    result = result.replace("\\mathsf", "")
    result = result.replace("\\mathcal", "")
    result = result.replace("\\begin{bmatrix}", "[")
    result = result.replace("\\end{bmatrix}", "]")
    result = result.replace("\\begin", "")
    result = result.replace("\\end", "")
    result = result.replace("\\left", "")
    result = result.replace("\\right", "")
    result = result.replace("\\operatorname", "")
    result = result.replace("\\mathrm", "")
    result = result.replace("\\boldsymbol", "")
    result = result.replace("\\quad", "    ")
    result = result.replace("\\qquad", "    ")
    result = result.replace("{aligned}", "")
    result = result.replace("{bmatrix}", "")
    result = result.replace("{}", "")
    result = result.replace("&", ", ")
    result = result.replace("\\\\", "], [")

    # convert remaining braces
    result = result.replace("{", "(")
    result = result.replace("}", ")")
    result = result.replace("\\(", "{")
    result = result.replace("\\)", "}")
    result = result.replace("\\", " ")
    result = re.sub(r'\(\s*', '(', result)
    result = re.sub(r'\s*\)', ')', result)
    result = re.sub(r'\[\s*', '[', result)
    result = re.sub(r'\s*\]', ']', result)

    # clean up extra spaces around operators
    result = re.sub(r'\s*,\s*', ', ', result)
    result = re.sub(r'\s*_', '_', result)
    result = re.sub(r'\s*\|', '|', result)
    result = re.sub(r'\s*\^', '^', result)
    result = re.sub(r"\s*'", "'", result)
    result = re.sub(r'\s*;', ';', result)
    result = re.sub(r'\s*,', ',', result)
    
    # done (whew!)
    return result.strip()
    
def number_headers(data: bytes) -> bytes:
    # Convert Markdown-style '#' headers in a bytes string into numbered headers (1.1. Header, etc.)
    lines = data.splitlines(keepends=True)
    counters = []  # header counters per level
    out = []
    for line in lines:
        # count leading #
        i = 0
        length = len(line)
        while i < length and line[i:i+1] == b'#':
            i += 1

        # valid header requires at least one '#' followed by a space
        if i > 0 and i < length and line[i:i+1] == b' ':
            level = i

            # safety: ensure counters list is long enough
            while len(counters) < level:
                counters.append(0)

            # increment this level and reset deeper levels
            counters[level - 1] += 1
            counters[level:] = []

            # build numbering prefix, e.g. b"1.2. "
            number = b'.'.join(str(n).encode('ascii') for n in counters) + b'. '
            out.append(to_bold(number) + line[i + 1:])
            
        else:
            out.append(line)

    return b''.join(out)
    
def bracket_count(line: bytes):
    # count header depth (used for section cleanup)
    for i in range(len(line)):
        if line[i] != b'#':
            return i
    return i

def simplify_wikipedia_page(
    page_name: str,
    omit_tags: Optional[List[str]] = None,
    omit_classes: Optional[Set[str]] = None,
    omit_sections: Optional[List[bytes]] = None
) -> bytes:
    # Download and simplify a Wikipedia page to (mostly) ASCII text.
    #   page_name: Name of the Wikipedia page (e.g., "Python_(programming_language)")
    #   omit_tags: List of HTML tag names to omit
    #   omit_classes: Set of CSS class names to omit
    #   omit_sections: List of section names to omit entirely
    
    # defaults
    if omit_tags is None:
        omit_tags = ["figure", "table", "style", "script"]
    if omit_classes is None:
        omit_classes = {"editsection", "reference", "references", "hatnote", "noprint", "navbox", "reflist", "mw-references-wrap"}
    if omit_sections is None:
        omit_sections = [b"See also", b"Notes", b"References", b"Sources", b"Citations", b"Further reading", b"Bibliography", b"Sources cited", b"External links", b"Gallery", b"Notes and references"]
    
    # download
    url = f"https://en.wikipedia.org/wiki/{page_name}"
    headers = {'User-Agent': 'EncontraBot/1.0'}
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    
    # extract main page content
    soup = BeautifulSoup(response.text, 'html.parser')
    content = soup.find(id="mw-content-text")
    if not content:
        raise ValueError("Could not find mw-content-text in the page")
    
    # remove tags/classes matching omit_tags and omit_classes
    for tag_name in omit_tags:
        for tag in content.find_all(tag_name):
            tag.decompose()
    for class_name in omit_classes:
        for tag in content.find_all(class_=class_name):
            tag.decompose()
    
    # extract LaTeX from math elements before processing
    for math_span in content.find_all('span', class_='mwe-math-element'):
        # find the fallback image with alt text containing LaTeX
        img = math_span.find('img', class_='mwe-math-fallback-image-inline')
        if not img:
            img = math_span.find('img', class_='mwe-math-fallback-image-display')
        if img and img.get('alt'):
            # replace the entire math span with a simpler ASCII version
            latex = img.get('alt')
            if latex[:15] == "{\\displaystyle ":
                latex = latex[15:-1]
            ascii_math = latex_to_ascii(latex)
            math_span.replace_with(ascii_math)
    
    # remove sections matching omit_sections
    for header in content.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6']):
        header_text = decode_bytes(header.get_text(strip=True))
        if header_text in omit_sections:
            # get the header level and find its container (might be wrapped in a div)
            header_level = int(header.name[1])
            
            # start from the header's parent (which might be mw-heading div) or the header itself
            if header.parent and 'mw-heading' in ' '.join(header.parent.get('class', [])):
                start_element = header.parent
            else:
                start_element = header
            
            # remove all following siblings until we hit a header of equal or higher level
            current = start_element.next_sibling
            while current:
                next_sibling = current.next_sibling
                
                # check if this element contains a header of equal or higher level
                should_stop = False
                if hasattr(current, 'name') and current.name:
                    # check if current element itself is a header
                    if current.name in ['h1', 'h2', 'h3', 'h4', 'h5', 'h6']:
                        if int(current.name[1]) <= header_level:
                            should_stop = True
                            
                    # check if current element contains a header (e.g., mw-heading div)
                    elif hasattr(current, 'find_all'):
                        for nested_header in current.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6'], recursive=False):
                            if int(nested_header.name[1]) <= header_level:
                                should_stop = True
                                break
                                
                        # check one level deep for mw-heading divs
                        if not should_stop and hasattr(current, 'children'):
                            for child in current.children:
                                if hasattr(child, 'find_all'):
                                    for nested_header in child.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6'], recursive=False):
                                        if int(nested_header.name[1]) <= header_level:
                                            should_stop = True
                                            break
                                if should_stop:
                                    break
                
                if should_stop:
                    break
                
                if hasattr(current, 'decompose'):
                    current.decompose()
                current = next_sibling
            
            # remove the header container itself
            start_element.decompose()
    
    # process content
    result = []
    _process_element(content, result, omit_classes, omit_tags)
    
    # join and clean up
    text = b"\n".join(result)
    text = _clean_whitespace(text)
    if text[-1] == 0x08:
        text = text[:-1]
    text = text.replace(b" ,", b",")
    text = text.replace(b" ;", b";")
    text = text.replace(b" )", b")")
    text = text.replace(b" .", b".")
    
    # clean up underlines
    text = text.replace(b"\n_", b"_")
    
    # remove empty sections
    lines = text.split(b"\n")
    truncated = 1
    while truncated:
        truncated = 0
        for i in range(len(lines) - 2):
            if len(lines[i]) > 0 and len(lines[i+2]) > 0:
                if lines[i][0] == b'#' and lines[i+2][0] == b'#':
                    if bracket_count(lines[i]) >= bracket_count(lines[i+2]):
                        lines[i] = b"!!!REMOVE!!!"
                        lines[i+1] = b"!!!REMOVE!!!"
                        truncated = 1
        if len(lines) > 2 and len(lines[-1]) > 0 and lines[-1][0] == b'#': # strip empty section at end
            lines[-2] = b"!!!REMOVE!!!"
            lines[-1] = b"!!!REMOVE!!!"
        lines = [line for line in lines if line != b"!!!REMOVE!!!"]
    text = b"\n".join(lines)
    
    # renumber headers
    text = number_headers(text)
    
    return text

def _process_inline_content(element, result: List[bytes], omit_classes: Set[str], omit_tags: List[str], list_context: Optional[List[int]] = None):
    # process inline content, handling special elements like sup/sub.
    if list_context is None:
        list_context = []
    
    for child in element.children:
        if isinstance(child, str):
            # direct text node
            ascii_text = decode_bytes(child)
            result.append(ascii_text)
            
        elif hasattr(child, 'name'):
            # has child, should it be omitted?
            if child.name in omit_tags:
                continue
            if child.get('class'):
                classes = set(child.get('class', []))
                if classes & omit_classes:
                    continue
            
            # handle special inline elements
            if child.name == 'a':
                text = _get_text(child)
                if text.strip():
                    result.append(text.strip())
            elif child.name == 'sup':
                text = _get_text(child)
                if text.strip():
                    result.append(b'^')
                    result.append(text.strip())
                    result.append(b' ')
            elif child.name == 'sub':
                text = _get_text(child)
                if text.strip():
                    result.append(b'_')
                    result.append(text.strip())
                    result.append(b' ')
            elif child.name == 'br':
                result.append(b"\n")
            else:
                # recurse into other inline elements (span, etc.)
                _process_inline_content(child, result, omit_classes, omit_tags, list_context)

def _process_element(element, result: List[str], omit_classes: Set[str], omit_tags: List[str], list_context: Optional[List[int]] = None):
    # Recursively process HTML elements and build the result.
    if list_context is None:
        list_context = []
        
    # skip some things
    if not hasattr(element, 'name') or element.name is None:
        return # skip text nodes
    if element.name in omit_tags:
        return # skip if omitted tag
    if element.get('class'):
        classes = set(element.get('class', []))
        if classes & omit_classes:
            return # skip if omitted class
    
    # wrap quotes/pre in underlines
    if element.name == "blockquote":
        text = _get_text(element).strip().split(b"\n")
        if len(text):
            result.extend([b"    \"" + line for line in text])
            result[-1] = result[-1] + b"\""
        return
    elif element.name == "pre":
        result.append(b"\n__________\n")
        text = _get_text(element)
        if text.strip():
            result.append(text.strip().replace(b"\n", b"\r")) # \r is converted to \n later, preserving literal newlines
        result.append(b"\n__________\n")
        return
        
    # handle element
    if element.name in ['h1', 'h2', 'h3', 'h4', 'h5', 'h6']:
        # header, format it for subsequent numbering
        level = int(element.name[1])
        text = _get_text(element)
        if text.strip():
            result.append(b"\n" + (b'#' * (level-1)) + b" " + to_bold(text.strip()) + b"\n")
        
    elif element.name == 'ul':
        # unordered list, add bullets
        for li in element.find_all('li', recursive=False):
            text = _get_text(li)
            if text.strip():
                result.append((b"  " * len(list_context)) + chr(127).encode("latin1") + b" " + text.strip())
    
    elif element.name == 'ol':
        # ordered list, number it
        new_context = list_context + [0]
        for i, li in enumerate(element.find_all('li', recursive=False), 1):
            new_context[-1] = i
            
            # get direct text of this li
            text_parts = []
            for child in li.children:
                if isinstance(child, str):
                    ascii_text = decode_bytes(child)
                    text_parts.append(ascii_text)
                elif child.name not in ['ul', 'ol']:
                    text_parts.append(_get_text(child))
            
            text = b''.join(text_parts).strip()
            if text:
                result.append((b"  " * (len(list_context))) + b'.'.join(str(x).encode() for x in new_context) + b". " + text)
            
            # process nested lists
            for nested in li.find_all(['ul', 'ol'], recursive=False):
                _process_element(nested, result, omit_classes, omit_tags, new_context)
    
    elif element.name in ['p', 'dd']:
        # paragraph, just collect inline content
        inline_result = []
        _process_inline_content(element, inline_result, omit_classes, omit_tags, list_context)
        text = b''.join(inline_result).strip()
        if text:
            result.append(b"\n" + text + b"\b")
    
    elif element.name in ['div', 'section', 'article']:
        # container, recursively process children
        for child in element.children:
            if hasattr(child, 'name'):
                _process_element(child, result, omit_classes, omit_tags, list_context)
    
    elif element.name == 'br':
        # newline
        result.append(b"\n")
    
    else:
        # something else, just extract text
        if hasattr(element, 'children'):
            for child in element.children:
                if hasattr(child, 'name'):
                    _process_element(child, result, omit_classes, omit_tags, list_context)

def _get_text(element) -> bytes:
    # Extract an element's text and convert to ASCII, handling links differently
    
    # link: process as link
    if element.name == 'a':
        return _process_link(element)
    
    # something else: process children recursively
    if hasattr(element, 'children'):
        # has children
        parts = []
        for child in element.children:
            if isinstance(child, str):
                # direct text node
                ascii_text = decode_bytes(child)
                parts.append(ascii_text)
            elif hasattr(child, 'name'):
                # has children - process recursively
                parts.append(_get_text(child))
        return b''.join(parts)
    else:
        # end text node
        text = str(element).encode("latin1")
        ascii_text = decode_bytes(text)
        return ascii_text

def _process_link(link_element) -> bytes:
    # Process an anchor tag, underlining if it's a Wikipedia self-link
    global whole_index

    # get link text
    href = link_element.get('href', '')
    link_text = link_element.get_text(strip=True)
    ascii_text = decode_bytes(link_text)
    
    # check if it's a self-link
    if href.startswith('/wiki/'):
        # extract page name from URL (everything after /wiki/)
        page_name = href[6:]  # Remove '/wiki/' prefix
        
        # decode URL encoding and replace underscores with spaces
        import urllib.parse
        decoded_page = urllib.parse.unquote(page_name)
        decoded_page = decoded_page.replace('_', ' ')
        
        # look up the page and, if indexed, add its ID as a link
        if decoded_page in whole_index:
            return b"!!!!" + str(whole_index.index(decoded_page)).encode("ascii") + b"!" + to_underline(ascii_text) + b"!!!!"
    
    # otherwise, just return the text
    return ascii_text

def _clean_whitespace(text: bytes) -> bytes:
    # Clean up excessive whitespace while preserving structure
    lines = text.split(b'\n')
    lines = [line for line in lines if len(line.strip())]
    return b'\n\n'.join(lines).strip().replace(b"\r", b"\n").replace(b"\n", b"\r\n").replace(b"\x08\r\n", b"\r\n")

def split_page(pages, PAGE_SIZE=12224):
    # Split a byte string into smaller pages based on PAGE_SIZE constraints
    # Returns a list of byte strings, each representing a subpage ending with \x00

    # remove trailing null bytes to start
    pages = pages.rstrip(b'\x00')    

    # if whole page fits, return as-is
    if len(pages) + 1 <= PAGE_SIZE:
        return [pages + b'\x00']
    
    # split into paragraphs using \r\n\r\n
    paragraphs = pages.split(b'\r\n\r\n')
    separator = b'\r\n\r\n'
    subpages = []
    current_page = b''
    i = 0
    while i < len(paragraphs):
        para = paragraphs[i]
        is_header = (para[0] >= 1 and para[0] <= 7) or (para[0] >= 253) # bold numbers
        
        # build what we'd add to current page
        if current_page:
            addition = separator + para
        else:
            addition = para
        would_fit = len(current_page) + len(addition) + 1 <= PAGE_SIZE
        
        # header: can it fit with the next paragraph?
        if is_header and i + 1 < len(paragraphs):
            next_para = paragraphs[i + 1]
            header_and_next = addition + separator + next_para
            both_fit = len(current_page) + len(header_and_next) + 1 <= PAGE_SIZE
            
            if both_fit:
                # can fit both header and next paragraph
                current_page += addition
                i += 1
                continue
            elif current_page:
                # can't fit both, but we have content already; end current page and move header to next page
                subpages.append(current_page)
                current_page = b''
                continue
            else:
                # can't fit both and page is empty; just emit header (body will be handled below)
                current_page += addition
                i += 1
                continue
        
        # not a header, or header at end with no following paragraph
        if would_fit:
            # fits on current page
            current_page += addition
            i += 1
            continue

        else:
            # can't fit, finalize current page
            if current_page:
                subpages.append(current_page)
                current_page = b''
            else:
                # single paragraph too large (occasionally happens with <pre>); split by line
                addition_lines = addition.split(b"\r\n")
                for line in addition_lines:
                    if len(current_page) + len(line) + 5 >= PAGE_SIZE:
                        subpages.append(current_page)
                        current_page = b''
                    current_page += b"\r\n" + line
                i += 1
    
    # add any remaining content
    if current_page:
        subpages.append(current_page)
    
    # add navigation markers and null terminators
    if len(subpages) > 1:
        result = []
        for i in range(len(subpages)):
            page = subpages[i]
            
            # add "(previous page)" prefix to all but first
            if i > 0:
                page = to_underline(b'(previous page)') + b'\r\n\r\n' + page
            
            # add "(next page)" suffix to all but last  
            if i < len(subpages) - 1:
                page = page + b'\r\n\r\n' + to_underline(b'(next page)')
            
            # finish
            page = page + b'\x00'
            result.append(page)
        return result
    
    # single page case - just add null terminator
    return [subpages[0] + b'\x00']
    