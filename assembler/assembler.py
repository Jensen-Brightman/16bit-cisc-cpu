import argparse
import os
import struct
import sys


class Instruction:
    instruction_list: list[Instruction] = []
    label_list: list[Line] = []
    org: int = 0

    def __init__(self, opcode: int, full_name: str, name: str,
                 arg_types: str, attrs_enb: list[str],
                 base_size: int, uses_relative_referencing: bool = False) -> None:
        Instruction._validate_arg_types(arg_types)
        Instruction._validate_atr_types(attrs_enb)

        self.opcode: int = opcode
        self.full_name = full_name
        self.name: str = name.lower()
        self.arg_types: str = arg_types
        self.attrs_enb: list[str] = attrs_enb
        self.base_size: int = base_size
        self.uses_relative_referencing: bool = uses_relative_referencing

    @staticmethod
    def _validate_arg_types(arg_types: str) -> None:
        for c in arg_types:
            if c in "rpi":
                continue
            raise ValueError("[INSTR TYPES] Args types must be 'RPI'")

    @staticmethod
    def _validate_atr_types(atr_types: list[str]) -> None:
        for atr in atr_types:
            if atr in ["byte", "flags", "carry"]:
                continue
            raise ValueError(
                "[INSTR TYPES] Attributes must be byte, flag or carry")

    @staticmethod
    def _seperate_parameters(args: list[str]) -> tuple[list[str], list[str]]:
        attr_list: list[str] = []
        arg_list:  list[str] = []
        for a in args:
            if a in ["byte", "flags", "carry"]:
                attr_list.append(a)
            else:
                arg_list.append(a)
        return attr_list, arg_list

    @staticmethod
    def tokenize(s: str) -> tuple[str, list[str], list[str]]:
        name = s.split(" ")[0]
        text_after_name = s[s.find(" "):] if s.find(" ") > -1 else ""
        og_parameters = [k for k in text_after_name.replace(",", " ").split(" ")
                         if k.replace(" ", "")]

        parameters: list[str] = []
        carry_negative = False
        for param in og_parameters:
            if "+" in param and param[0] != "+":
                parameters += [k.strip() for k in param.split("+")
                               if k.replace(" ", "") != ""]
            elif "-" in param and param[0] != "-":
                for i, k in enumerate(param.split("-")):
                    if k.replace(" ", "") == "":
                        continue
                    if i == 1:
                        parameters.append("-"+k.strip())
                    else:
                        parameters.append(k.strip())
            elif param.strip() != "+" and param.strip() != "-":
                if carry_negative:
                    parameters.append("-"+param.strip())
                else:
                    parameters.append(param.strip())
            elif param.strip() == "-":
                carry_negative = True

        attributes, arguments = Instruction._seperate_parameters(parameters)

        return name, attributes, arguments

    @staticmethod
    def is_immediate(s: str) -> bool:
        if s.isnumeric():
            return True
        if s.replace("0b", "").isnumeric():
            return True
        try:
            _ = int(s, 16)
            return True
        except:
            return False

    @staticmethod
    def check_label_exists(label: str) -> bool:
        for l in Instruction.label_list:
            if l.label_argument != label:
                continue
            return True

        raise Exception("Label isn't defined")

    @staticmethod
    def get_label_ln_with_name(label: str) -> Line | None:
        for l in Instruction.label_list:
            if l.label_argument != label:
                continue
            return l
        return None

    @staticmethod
    def extract_arg_type(arg: str) -> str:
        if arg[0] == "r":
            return "r"
        if arg[:2] == "[r" and arg[-1] == "]":
            return "p"
        if arg[:2] == "[r":
            return "p"
        if arg[-1] == "]":
            return "i"
        if Instruction.is_immediate(arg) or Instruction.check_label_exists(arg):
            return "i"

        raise ValueError("Argument type not recognised")

    def compare(self, name: str, attrs: list[str], args: list[str]) -> bool:
        if name != self.name:
            return False

        arg_types = "".join([Instruction.extract_arg_type(k) for k in args])
        if arg_types != self.arg_types:
            return False

        for a in attrs:
            if a in self.attrs_enb:
                continue
            raise Exception(f"Attribute type '{a}' not valid for {self}")

        return True

    @staticmethod
    def find_instruction(instr_string: str) -> Instruction | None:
        tokens = Instruction.tokenize(instr_string)
        for instr in Instruction.instruction_list:
            if not instr.compare(*tokens):
                continue
            return instr

        if instr_string.replace(" ", "") != "" and instr_string[instr_string.find(":")+1:].replace(" ", "") != "":
            raise Exception(f"Unrecognised instruction: '{instr_string}'")

        return None

    @staticmethod
    def immediate_to_int(immediate: str) -> int:
        if immediate[-1] == "]":
            immediate = immediate[:-1]

        negative = immediate[0] == '-'
        if negative:
            immediate = immediate[1:]

        if immediate.isnumeric():
            return int(immediate) * (-1 if negative else 1)
        if immediate[:2] == "0b":
            return int(immediate[2:], 2) * (-1 if negative else 1)
        if immediate[:2] == "0x":
            return int(immediate[2:], 16) * (-1 if negative else 1)

    @staticmethod
    def immediate_creates_offest(immediate: int) -> 0 | 1:
        if immediate >= 0:
            return 1 if immediate > 255 else 0
        return 1 if immediate < -128 else 0

    @staticmethod
    def immediate_requires_word(immediate: int) -> bool:
        if immediate >= 0:
            return True if immediate > 255 else False
        return True if immediate < -128 else False

    def __repr__(self):
        return f'{self.opcode}_{self.full_name}_{self.arg_types}{"_" if self.attrs_enb else ""}{"".join([k[0] for k in self.attrs_enb])}'


class Line:
    def __init__(self, line: str, ln_num: int):
        self.orginal_string: str = line
        self.string_content: str = Line._process_string(line)
        self.raw_bytes: bytearray = None
        self.line_number: int = ln_num
        self.related_instr: Instruction = None
        self.args: list = None
        self.attributes: list[str] = None
        self.label_argument: str = None
        self.label_reference: Line = None
        self.immediate_refernce_size = 1

        self.check_for_labels()

    @staticmethod
    def _remove_comments(s: str) -> str:
        return s[:s.find(";")] if ";" in s else s

    @staticmethod
    def _remove_leading_space(s: str) -> str:
        if s == "":
            return s

        k = 0
        for i, c in enumerate(s):
            if c == " ":
                continue
            k = i
            break
        return s[i:]

    @staticmethod
    def _process_string(s: str) -> str:
        s = Line._remove_comments(s)
        s = Line._remove_leading_space(s)
        s = s.lower()
        return s

    @staticmethod
    def from_source(source: str) -> list[Line]:
        lns = source.split("\n")
        return [Line(s, n) for n, s in enumerate(lns)]

    def check_for_references(self) -> None:
        if self.get_immediate_index() == -1:
            return
        reference = self.get_immediate_value()
        if (not reference) or Instruction.is_immediate(reference):
            return

        self.label_reference = Instruction.get_label_ln_with_name(reference)
        
    def check_for_labels(self) -> None:
        if not ":" in self.string_content:
            return

        label = self.string_content[:self.string_content.find(":")]
        if " " in label:
            raise Exception("Labels cannot contain spaces")
        if label == "":
            raise Exception("Label is empty")
        if label[0].isnumeric():
            raise Exception("Label cannot start with a number")

        self.label_argument = label
        Instruction.label_list.append(self)

    def check_for_directives(self) -> bool:
        if self.string_content == "" or self.string_content[0] != ".":
            return False

        parts = self.string_content.split(None, 1)
        directive = parts[0]
        arg_str = parts[1].strip() if len(parts) > 1 else ""
        self.raw_bytes = bytearray()

        if directive == ".byte":
            for a in arg_str.split(","):
                num = Instruction.immediate_to_int(a.strip())
                self.raw_bytes.extend(struct.pack(
                    "<B" if num >= 0 else "<b", num))
        elif directive == ".word":
            for a in arg_str.split(","):
                num = Instruction.immediate_to_int(a.strip())
                self.raw_bytes.extend(struct.pack(
                    "<H" if num >= 0 else "<h", num))
        elif directive == ".org":
            if Instruction.is_immediate(arg_str) or Instruction.org != 0:
                Instruction.org = Instruction.immediate_to_int(arg_str)
            else:
                raise Exception(".org directive malformed, or used more than once")

        else:
            raise Exception(f"Unknown directive '{directive}'")

        return True

    def get_immediate_index(self) -> int:
        return self.related_instr.arg_types.find("i")

    def get_immediate_value(self) -> any:
        return self.args[self.related_instr.arg_types.find("i")]

    def get_referenced_ln(self) -> Line:
        return self.label_reference
        # return Instruction.get_label_ln_with_name(self.get_immediate_value())

    def evaluate_label_arguments(self, lines: list[Line]) -> bool:
        """
        Returns if any changes where made to the lines refernce size.
        """
        if not (self.related_instr and "i" in self.related_instr.arg_types):
            return        

        target_line = self.get_referenced_ln()
        if not target_line:
            return

        offset_calculated_bytes: int = 0

        if self.related_instr.uses_relative_referencing:
            start_line = self
            if start_line.line_number > target_line.line_number:
                for i in range(target_line.line_number + 1, start_line.line_number):
                    offset_calculated_bytes -= lines[i].byte_size()

            if start_line.line_number < target_line.line_number:
                for i in range(start_line.line_number + 1, target_line.line_number):
                    offset_calculated_bytes += lines[i].byte_size()

                offset_calculated_bytes += Instruction.org

                if start_line.related_instr:
                    ln_base_size = start_line.related_instr.base_size
                else:
                    ln_base_size = 0

                minimum_offset = offset_calculated_bytes + ln_base_size
                if -128 <= minimum_offset and minimum_offset <= 255:
                    offset_calculated_bytes += ln_base_size
                else:
                    offset_calculated_bytes += ln_base_size + 1
        else:
            offset_calculated_bytes = Instruction.org + sum(
                line.byte_size() for line in lines[:target_line.line_number])

        self.args[self.get_immediate_index()] = str(offset_calculated_bytes)
        if self.immediate_refernce_size != 2:
            self.immediate_refernce_size += Instruction.immediate_creates_offest(
                offset_calculated_bytes)
            return self.immediate_refernce_size == 2

        return False

    def byte_size(self) -> int:
        if self.raw_bytes != None:
            return len(self.raw_bytes)
        if not self.related_instr:
            return 0
        if not "i" in self.related_instr.arg_types:
            return self.related_instr.base_size

        if self.get_referenced_ln():
            return self.related_instr.base_size + self.immediate_refernce_size - 1
        else:
            return self.related_instr.base_size + Instruction.immediate_creates_offest(
                Instruction.immediate_to_int(self.get_immediate_value()))

    def generate_binary(self, verbose: bool) -> bytearray:
        if self.raw_bytes != None:
            if verbose:
                print(f"{self.line_number+1}: " +
                      " ".join([f"{b:02X}" for b in self.raw_bytes]))
            return self.raw_bytes

        if not self.related_instr:
            return

        binary: bytearray = bytearray()

        imm = None
        if "i" in self.related_instr.arg_types:
            imm = Instruction.immediate_to_int(
                self.get_immediate_value().replace("]", "").strip())

        # Opcode
        binary.extend(struct.pack("<B", self.related_instr.opcode))

        # Attributes
        a0 = "byte" in self.attributes or "flags" in self.attributes  # means byte or flags
        a1 = "carry" in self.attributes  # update use carry
        a2 = True if (imm != None and imm < 0) else False
        a3 = (not Instruction.immediate_requires_word(
            imm)) if imm != None else False

        packed_attributes = (int(a3) << 3) | (
            int(a2) << 2) | (int(a1) << 1) | int(a0)

        if not (self.related_instr.attrs_enb == [] and imm == None):
            binary.extend(struct.pack("<B", packed_attributes))

        # Registers
        registers = []
        for a in self.args:
            if not (a[0] == "r" or a[:2] == "[r"):
                continue
            a = a.replace("[", "").replace("]", "").replace("r", "").strip()
            registers.append(Instruction.immediate_to_int(a))

        match self.related_instr.arg_types.count("r") + self.related_instr.arg_types.count("p"):
            case 1:
                binary.extend(struct.pack("<B", registers[0]))

            case 2:
                combined_byte = (registers[0] << 4) | registers[1]
                binary.extend(struct.pack("<B", combined_byte))

            case 3:
                combined_byte = (registers[0] << 4) | registers[1]
                binary.extend(struct.pack("<B", combined_byte))
                binary.extend(struct.pack("<B", registers[2]))

        # Immediates
        if imm != None:
            if Instruction.immediate_requires_word(imm):
                binary.extend(struct.pack("<H" if imm >= 0 else "<h", imm))
            else:
                binary.extend(struct.pack("<B" if imm >= 0 else "<b", imm))

        if verbose:
            print(f"{self.line_number+1}: " +
                  " ".join([f"{b:02X}" for b in binary]))

        if len(binary) != self.byte_size():
            raise Exception(
                "Error generating binary: generated binary doesn't math instruction length")

        return binary

    def __repr__(self):
        label_comment = f' ({self.label_argument})' if self.label_argument else ""
        reference_comment = f' (ref. ln {self.label_reference.line_number+1})' if self.label_reference else ""
        try:
            self_size = self.byte_size()
        except Exception as e:
            self_size = "-"

        if self.related_instr:
            return f'{self.line_number+1}: {self.orginal_string}  -> ({self.related_instr}) Atrs: {
                self.attributes}, Args: {self.args}, Size: {self_size}{label_comment}{reference_comment}'
        return f'{self.line_number+1}: {self.orginal_string}{label_comment}'


if __name__ == "__main__":
    Instruction.instruction_list = [
        # Memory/Registers
        Instruction(0, "OP_MOV_REG", "mov", "rr", ["byte"], 3),
        Instruction(1, "OP_MOV_IMM", "mov", "ri", [], 4),
        Instruction(2, "OP_LOAD", "mov", "rp", ["byte"], 3),
        Instruction(3, "OP_LOAD_IDX", "mov", "rpi", ["byte"], 4),
        Instruction(4, "OP_STORE", "mov", "pr", ["byte"], 3),
        Instruction(5, "OP_STORE_IDX", "mov", "pir", ["byte"], 4),
        Instruction(6, "OP_MEM2MEM", "mov", "pp", ["byte"], 3),
        Instruction(7, "OP_MEM2MEM_IDX", "mov", "ppi", ["byte"], 4),

        # Control Registers
        Instruction(8, "OP_MTC", "mtc", "rr", [], 2),
        Instruction(9, "OP_MFC", "mfc", "rr", [], 2),

        # Arithmetic/Logic
        Instruction(10, "OP_ADD", "add", "rrr", ["flags", "carry"], 4),
        Instruction(11, "OP_ADD_IMM", "add", "rri", ["flags", "carry"], 4),
        Instruction(12, "OP_SUB", "sub", "rrr", ["flags", "carry"], 4),
        Instruction(13, "OP_SUB_IMM", "sub", "rri", ["flags", "carry"], 4),
        Instruction(14, "OP_AND", "and", "rrr", ["flags"], 4),
        Instruction(15, "OP_AND", "and", "rri", ["flags"], 4),
        Instruction(16, "OP_OR", "or", "rrr", ["flags"], 4),
        Instruction(17, "OP_OR_IMM", "or", "rri", ["flags"], 4),
        Instruction(18, "OP_NOT", "not", "rr", ["flags"], 3),
        Instruction(19, "OP_LSR", "lsr", "ri", ["flags"], 4),
        Instruction(20, "OP_LSL", "lsl", "ri", ["flags"], 4),
        Instruction(21, "OP_CMP", "cmp", "rr", [], 2),
        Instruction(22, "OP_CMP_IMM", "cmp", "ri", [], 4),

        # Interupts
        Instruction(23, "OP_SYS", "sys", "", [], 1),
        Instruction(24, "OP_ERET", "eret", "", [], 1),
        Instruction(25, "OP_CLI", "cli", "", [], 1),
        Instruction(26, "OP_STI", "sti", "", [], 1),

        # MMU
        Instruction(27, "OP_MMU_ON", "mmuon", "", [], 1),
        Instruction(28, "OP_MMU_OFF", "mmuoff", "", [], 1),
        Instruction(29, "OP_MMU_PTE", "pte", "rr", [], 2),
        Instruction(30, "OP_MMU_FLUSH", "mfl", "", [], 1),

        # Branching
        Instruction(31, "OP_BRANCH", "b", "r", [], 2),
        Instruction(32, "OP_BRANCH_IMM", "b", "i", [], 3, True),
        Instruction(33, "OP_BEQ_IMM", "beq", "i", [], 3, True),
        Instruction(34, "OP_BNE_IMM", "bne", "i", [], 3, True),
        Instruction(35, "OP_BLT_IMM", "blt", "i", [], 3, True),
        Instruction(36, "OP_BGT_IMM", "bgt", "i", [], 3, True),

        # Stack
        Instruction(37, "OP_PUSH", "push", "r", [], 2),
        Instruction(38, "OP_POP", "pop", "r", [], 2),
        Instruction(39, "OP_CALL", "call", "r", [], 2),
        Instruction(40, "OP_CALL_IMM", "call", "i", [], 3),
        Instruction(41, "OP_RET", "ret", "", [], 1),

        Instruction(42, "OP_HLT", "hlt", "", [], 1),
    ]

    parser = argparse.ArgumentParser(
        description="Assembler for custom 16-bit 'CISC' CPU")
    parser.add_argument("input", help="Path to the input .asm source file")
    parser.add_argument(
        "-o", "--output",
        default="result.bin",
        help="Path for the output binary file (default: result.bin)"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print Pass 1 and Pass 2 debug output"
    )
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found.", file=sys.stderr)
        sys.exit(1)

    source = ""
    with open(args.input, "r") as file:
        source = file.read()

    line_list = Line.from_source(source)
    if args.verbose:
        print("[Pass #1]")
    for ln in line_list:
        try:
            if ln.check_for_directives():
                if args.verbose:
                    print(ln)
                continue

            ln.related_instr = Instruction.find_instruction(ln.string_content)
            if ln.related_instr:
                tokens = Instruction.tokenize(ln.string_content)
                ln.attributes = tokens[1]
                ln.args = tokens[2]
                ln.check_for_references()

            if args.verbose:
                print(ln)

        except Exception as e:
            # raise Exception(f"[ln {ln.line_number}] {e}") from None
            raise Exception(f"[ln {ln.line_number+1}] {e}")

    if args.verbose:
        print("[Pass #2]")
    i: int = 0
    while i < len(line_list):
        try:
            ln = line_list[i]
            ref_upd = ln.evaluate_label_arguments(line_list)
            if ref_upd:
                i = 0
                continue
            i += 1
        except Exception as e:
            # raise Exception(f"[ln {ln.line_number}] {e}") from None
            raise Exception(f"[ln {ln.line_number+1}] {e}")

    binary_result: bytearray = bytearray()
    for ln in line_list:
        if (n := ln.generate_binary(args.verbose)):
            binary_result += n

    with open(args.output, "wb") as file:
        file.write(binary_result)

    print(f"Success: Assembled {len(binary_result)} bytes to '{args.output}'")