import os, sys
import argparse
import hashlib
import py3compat
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection
from elftools.elf.relocation import RelocationSection
from elftools.elf.descriptions import describe_reloc_type

def colored(text, *args, **kargs):
    return text
if sys.stdout.isatty():
    try:
        from colorama import init as cl_init
        from termcolor import colored
        cl_init(autoreset = True)
    except:
        pass

################################################################################
# Console output functions and other helpers
################################################################################

def bold(s, col):
    return colored(s, col, attrs = ['bold'])

def yellow(s):
    return bold(s, 'yellow')

def red(s):
    return bold(s, 'red')

def blue(s):
    return bold(s, 'blue')

################################################################################
# Helpers
################################################################################

def execute(cmd, args, exit_on_error = True):
    if not args.no_verbose:
        print ("[Executing] " + cmd)
    res = os.system(cmd)
    if (res != 0) and exit_on_error:
        sys.exit(1)
    return res

def error(msg, code = 1):
    sys.stderr.write(red("Error! " + msg + "\n"))
    sys.exit(code)

def warn(msg):
    sys.stderr.write(red("WARNING: " + msg + "\n"))

def check(cond, msg, code = 1):
    if not cond:
        error(msg, code)

def split_fname(name):
    temp, ext = os.path.splitext(name)
    path, fname = os.path.split(temp)
    return path, fname, ext

def change_ext(name, new_ext):
    path, fname, ext = split_fname(name)
    return os.path.join(path, fname + new_ext)

def get_wrapped_name(n):
    nb = py3compat.str2bytes(n)
    s = hashlib.md5(nb).hexdigest()
    return "__%s__%s" % (s[:9], n)

debug_col = 'blue'
def debug(msg, args, col = None):
    if not args.no_debug:
        col = col or debug_col
        print ("%s %s" % (red("[debug]"), bold(msg, col)))

def set_debug_col(col = None):
    global debug_col
    debug_col = col or 'blue'

def print_list(l, header, args, col = None):
    if l:
        col = col or debug_col
        debug("%s %s" % (bold(header, col), bold(", ".join(l), col)), args, col)

def round_to(n, sz):
    return (n + sz - 1) & ~(sz - 1);

# http://stackoverflow.com/a/4999321
class RejectingDict(dict):
    def __setitem__(self, k, v):
        if k in self.keys():
            raise ValueError("Key '%s' is already present" % str(k))
        else:
            return super(RejectingDict, self).__setitem__(k, v)

def get_arg_parser(desc):
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('--no-verbose', dest="no_verbose", action="store_true", help="Don't show verbose commands (default: false)")
    parser.add_argument('--no-debug', dest="no_debug", action="store_true", help="Don't show debug data (default: false)")
    return parser

################################################################################
# ELF manipulation
################################################################################

# Iterate through the input ELF, returning all the symbols found
# and the associated data
def get_symbols_in_elf(obj):
    syms = {}
    with open(obj, "rb") as f:
        elf = ELFFile(f)
        for section in elf.iter_sections():
            if not isinstance(section, SymbolTableSection):
                continue
            for symbol in section.iter_symbols():
                sdata = {}
                sdata["type"] = symbol['st_info']['type']
                sdata["bind"] = symbol['st_info']['bind']
                sdata["size"] = symbol['st_size']
                sdata["visibility"] = symbol['st_other']['visibility']
                sdata["section"] = symbol['st_shndx']
                try:
                    sdata["section"] = int(sdata["section"])
                except:
                    pass
                sdata["value"] = int(symbol['st_value'])
                syms[str(symbol.name)] = sdata
    return syms

# TODO: also consider weak functions here?
def get_public_functions_in_object(obj):
    return [s for s, d in get_symbols_in_elf(obj).items() if d["type"] == "STT_FUNC" and d["bind"] == "STB_GLOBAL"]

def get_relocations_in_elf(obj):
    rels = []
    with open(obj, "rb") as f:
        elf = ELFFile(f)
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            symtable = elf.get_section(section['sh_link'])
            for rel in section.iter_relocations():
                if rel['r_info_sym'] == 0:
                    continue
                rdata = {}
                rdata["offset"] = int(rel['r_offset'])
                rdata["info"] = rel['r_info']
                rdata["type"] = describe_reloc_type(rel['r_info_type'], elf)
                symbol = symtable.get_symbol(rel['r_info_sym'])
                if symbol['st_name'] == 0:
                    symsec = elf.get_section(symbol['st_shndx'])
                    rdata["name"] = str(symsec.name)
                else:
                    rdata["name"] = str(symbol.name)
                rdata["value"] = symbol["st_value"]
                rels.append(rdata)
    return rels

def get_section_in_elf(obj, section_name):
    sect = {}
    with open(obj, "rb") as f:
        elf = ELFFile(f)
        for i, section in enumerate(elf.iter_sections()):
            if section.name == section_name:
                sect["name"] = section_name
                sect["addr"] = int(section['sh_addr'])
                sect["offset"] = int(section['sh_offset'])
                sect["size"] = int(section['sh_size'])
                sect["data"] = section.data()
                sect["index"] = i
                check(len(sect["data"]) == sect["size"], "Section '%s' real and declared data are different" % section_name)
                break
        else:
            error("Section '%s' not found" % section_name)
    return sect
