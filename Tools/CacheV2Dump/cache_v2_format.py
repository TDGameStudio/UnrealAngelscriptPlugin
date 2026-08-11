"""Bounded, read-only decoders for the frozen AngelScript Cache V2 wire formats."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct
import zlib

try:
    from blake3 import blake3 as _accelerated_blake3
except ImportError:  # The tool intentionally remains dependency-free.
    _accelerated_blake3 = None


MASK32 = 0xFFFFFFFF
BLAKE3_IV = (
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
)
MSG_PERMUTATION = (2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8)
CHUNK_START = 1
CHUNK_END = 2
PARENT = 4
ROOT = 8
BLOCK_LEN = 64
CHUNK_LEN = 1024

PACK_MAGIC = b'UEASCV2P'
MANIFEST_MAGIC = b'UEASCV2M'
POINTER_MAGIC = b'UEASCV2C'
PACK_HEADER_SIZE = 32
PACK_INDEX_ENTRY_SIZE = 96
MANIFEST_ROOT_SIZE = 65
MANIFEST_RECORD_SIZE = 122
MAX_PACK_BYTES = 128 * 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024 * 1024
MAX_RECORD_BYTES = 64 * 1024 * 1024
MAX_ENTRIES = 262144
MAX_STRING_BYTES = 1024 * 1024

RECORD_KINDS = {
    1: 'SourceIndex',
    2: 'ModuleInterface',
    3: 'TypeSchema',
    4: 'ModuleState',
    5: 'FunctionBody',
    6: 'DebugSidecar',
    7: 'ModuleSnapshot',
}
RECORD_KIND_VALUES = {value.lower(): key for key, value in RECORD_KINDS.items()}
CODECS = {0: 'None', 1: 'Zlib'}
DEPENDENCY_KINDS = {
    1: 'Import',
    2: 'Declaration',
    3: 'Signature',
    4: 'Inheritance',
    5: 'ValueLayout',
    6: 'PropertyLayout',
    7: 'GlobalStorage',
    8: 'HardValue',
    9: 'Initializer',
    10: 'CompileOption',
    11: 'EnvironmentAbi',
    12: 'FunctionContent',
}
REFERENCE_KINDS = {
    1: 'ScriptModule',
    2: 'ScriptType',
    3: 'ScriptFunction',
    4: 'ScriptGlobal',
    5: 'ScriptProperty',
    6: 'ScriptImport',
    7: 'EnvironmentSymbol',
    8: 'CanonicalName',
    9: 'StringLiteral',
}
TYPE_KINDS = {
    1: 'Class', 2: 'Struct', 3: 'Interface', 4: 'Enum',
    5: 'Delegate', 6: 'Typedef', 7: 'Funcdef',
}
TYPE_RELATION_KINDS = {
    1: 'Base', 2: 'ShadowSuper', 3: 'CodeSuper',
    4: 'ImplementedInterface', 5: 'Compose',
}
TYPE_LAYOUT_INPUT_KINDS = {1: 'BaseType', 2: 'CodeRoot', 3: 'StructHeader'}
METHOD_SLOT_KINDS = {
    1: 'LocalMethod', 2: 'VirtualDeclaration',
    3: 'VirtualOverride', 4: 'Inherited',
}
BEHAVIOR_KINDS = {
    1: 'Construct', 2: 'ListConstruct', 3: 'Destruct', 4: 'Factory',
    5: 'ListFactory', 6: 'AddRef', 7: 'Release', 8: 'GetWeakRefFlag',
    9: 'TemplateCallback', 10: 'GetRefCount', 11: 'SetGcFlag',
    12: 'GetGcFlag', 13: 'EnumRefs', 14: 'ReleaseRefs', 15: 'Copy',
    16: 'CopyConstruct', 17: 'CopyFactory',
}
REFLECTION_KINDS = {
    1: 'None', 2: 'UClass', 3: 'UStruct', 4: 'UEnum', 5: 'UDelegate',
}
DECLARATION_KINDS = {1: 'Type', 2: 'Function', 3: 'Global', 4: 'Property'}
DECLARATION_SLOT_KINDS = {
    1: 'Declaration', 2: 'Function', 3: 'VirtualFunction', 4: 'Import',
}
PARAMETER_PASSING_KINDS = {
    1: 'Value', 2: 'InReference', 3: 'OutReference', 4: 'InOutReference',
}
POINTER_KINDS = {
    'Current.ascurrent': (1, 'Current'),
    'Previous.ascurrent': (2, 'Previous'),
    'PendingColdStart.ascurrent': (3, 'PendingColdStart'),
}


class CacheDumpError(Exception):
    def __init__(
        self,
        code: str,
        stage: str,
        message: str,
        *,
        path: str = '',
        offset: int | None = None,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.stage = stage
        self.message = message
        self.path = path
        self.offset = offset

    def as_dict(self) -> dict[str, object]:
        result: dict[str, object] = {
            'code': self.code,
            'stage': self.stage,
            'message': self.message,
        }
        if self.path:
            result['path'] = self.path
        if self.offset is not None:
            result['offset'] = self.offset
        return result


def _rotate_right(value: int, count: int) -> int:
    return ((value >> count) | (value << (32 - count))) & MASK32


def _g(state: list[int], a: int, b: int, c: int, d: int, x: int, y: int) -> None:
    state[a] = (state[a] + state[b] + x) & MASK32
    state[d] = _rotate_right(state[d] ^ state[a], 16)
    state[c] = (state[c] + state[d]) & MASK32
    state[b] = _rotate_right(state[b] ^ state[c], 12)
    state[a] = (state[a] + state[b] + y) & MASK32
    state[d] = _rotate_right(state[d] ^ state[a], 8)
    state[c] = (state[c] + state[d]) & MASK32
    state[b] = _rotate_right(state[b] ^ state[c], 7)


def _round(state: list[int], message: list[int]) -> None:
    _g(state, 0, 4, 8, 12, message[0], message[1])
    _g(state, 1, 5, 9, 13, message[2], message[3])
    _g(state, 2, 6, 10, 14, message[4], message[5])
    _g(state, 3, 7, 11, 15, message[6], message[7])
    _g(state, 0, 5, 10, 15, message[8], message[9])
    _g(state, 1, 6, 11, 12, message[10], message[11])
    _g(state, 2, 7, 8, 13, message[12], message[13])
    _g(state, 3, 4, 9, 14, message[14], message[15])


def _compress(
    chaining_value: tuple[int, ...],
    block_words: tuple[int, ...],
    counter: int,
    block_length: int,
    flags: int,
) -> tuple[int, ...]:
    state = list(chaining_value) + list(BLAKE3_IV[:4]) + [
        counter & MASK32,
        (counter >> 32) & MASK32,
        block_length,
        flags,
    ]
    message = list(block_words)
    for _ in range(7):
        _round(state, message)
        message = [message[index] for index in MSG_PERMUTATION]
    return tuple(
        [state[index] ^ state[index + 8] for index in range(8)]
        + [state[index + 8] ^ chaining_value[index] for index in range(8)]
    )


def _block_words(block: bytes) -> tuple[int, ...]:
    return struct.unpack('<16I', block.ljust(BLOCK_LEN, b'\0'))


@dataclass(frozen=True)
class _Blake3Output:
    input_cv: tuple[int, ...]
    block_words: tuple[int, ...]
    counter: int
    block_length: int
    flags: int

    def chaining_value(self) -> tuple[int, ...]:
        return _compress(
            self.input_cv,
            self.block_words,
            self.counter,
            self.block_length,
            self.flags,
        )[:8]

    def root_digest(self) -> bytes:
        words = _compress(
            self.input_cv,
            self.block_words,
            0,
            self.block_length,
            self.flags | ROOT,
        )
        return struct.pack('<16I', *words)[:32]


def _chunk_output(chunk: bytes, chunk_counter: int) -> _Blake3Output:
    chaining_value = BLAKE3_IV
    block_count = max(1, (len(chunk) + BLOCK_LEN - 1) // BLOCK_LEN)
    for block_index in range(block_count - 1):
        block = chunk[block_index * BLOCK_LEN:(block_index + 1) * BLOCK_LEN]
        flags = CHUNK_START if block_index == 0 else 0
        chaining_value = _compress(
            chaining_value,
            _block_words(block),
            chunk_counter,
            BLOCK_LEN,
            flags,
        )[:8]
    final_start = (block_count - 1) * BLOCK_LEN
    final_block = chunk[final_start:final_start + BLOCK_LEN]
    flags = CHUNK_END
    if block_count == 1:
        flags |= CHUNK_START
    return _Blake3Output(
        chaining_value,
        _block_words(final_block),
        chunk_counter,
        len(final_block),
        flags,
    )


def _parent_output(left: tuple[int, ...], right: tuple[int, ...]) -> _Blake3Output:
    return _Blake3Output(BLAKE3_IV, left + right, 0, BLOCK_LEN, PARENT)


def blake3_digest(data: bytes) -> bytes:
    """Return the standard unkeyed BLAKE3-256 digest without third-party modules."""
    if _accelerated_blake3 is not None:
        return _accelerated_blake3(data).digest(length=32)
    chunks = [data[index:index + CHUNK_LEN] for index in range(0, len(data), CHUNK_LEN)]
    if not chunks:
        chunks = [b'']
    stack: list[tuple[int, ...]] = []
    for chunk_index, chunk in enumerate(chunks[:-1]):
        new_cv = _chunk_output(chunk, chunk_index).chaining_value()
        total_chunks = chunk_index + 1
        while total_chunks & 1 == 0:
            new_cv = _parent_output(stack.pop(), new_cv).chaining_value()
            total_chunks >>= 1
        stack.append(new_cv)
    output = _chunk_output(chunks[-1], len(chunks) - 1)
    for left_cv in reversed(stack):
        output = _parent_output(left_cv, output.chaining_value())
    return output.root_digest()


def record_content_hash(kind: int, canonical_payload: bytes) -> bytes:
    if kind not in RECORD_KINDS:
        raise CacheDumpError('UNKNOWN_RECORD_KIND', 'record-id', f'Unknown record kind {kind}.')
    semantic_header = b'UEAS-CACHE-RECORD\0' + struct.pack(
        '<IBQ', 2, kind, len(canonical_payload))
    return blake3_digest(semantic_header + canonical_payload)


def _read_bounded(path: Path, maximum: int, stage: str) -> bytes:
    try:
        stat = path.stat()
    except OSError as error:
        raise CacheDumpError('INPUT_READ', stage, str(error), path=path.name) from error
    if stat.st_size > maximum:
        raise CacheDumpError(
            'FILE_TOO_LARGE', stage,
            f'{path.name} is {stat.st_size} bytes; limit is {maximum}.', path=path.name)
    try:
        return path.read_bytes()
    except OSError as error:
        raise CacheDumpError('INPUT_READ', stage, str(error), path=path.name) from error


class Reader:
    def __init__(self, data: bytes, stage: str, path: str = '') -> None:
        self.data = data
        self.stage = stage
        self.path = path
        self.offset = 0

    def _take(self, size: int) -> bytes:
        start = self.offset
        end = start + size
        if size < 0 or end < start or end > len(self.data):
            raise CacheDumpError(
                'OUT_OF_BOUNDS', self.stage,
                f'Need {size} bytes at offset {start}, file size is {len(self.data)}.',
                path=self.path, offset=start)
        self.offset = end
        return self.data[start:end]

    def u8(self) -> int:
        return self._take(1)[0]

    def u32(self) -> int:
        return struct.unpack('<I', self._take(4))[0]

    def u64(self) -> int:
        return struct.unpack('<Q', self._take(8))[0]

    def i32(self) -> int:
        return struct.unpack('<i', self._take(4))[0]

    def hash(self) -> str:
        return self._take(32).hex()

    def record_id(self) -> dict[str, object]:
        kind = self.u8()
        if kind not in RECORD_KINDS:
            raise CacheDumpError(
                'UNKNOWN_RECORD_KIND', self.stage, f'Unknown record kind {kind}.',
                path=self.path, offset=self.offset - 1)
        content_hash = self.hash()
        return {
            'record_kind_value': kind,
            'record_kind': RECORD_KINDS[kind],
            'content_hash': content_hash,
            'record_id': f'{kind:02x}{content_hash}',
        }

    def string(self) -> str:
        length_offset = self.offset
        length = self.u32()
        if length > MAX_STRING_BYTES:
            raise CacheDumpError(
                'STRING_TOO_LARGE', self.stage,
                f'String length {length} exceeds {MAX_STRING_BYTES}.',
                path=self.path, offset=length_offset)
        encoded = self._take(length)
        if b'\0' in encoded:
            raise CacheDumpError(
                'EMBEDDED_NUL', self.stage, 'String contains an embedded NUL.',
                path=self.path, offset=length_offset + 4)
        try:
            return encoded.decode('utf-8')
        except UnicodeDecodeError as error:
            raise CacheDumpError(
                'INVALID_UTF8', self.stage, str(error), path=self.path,
                offset=length_offset + 4 + error.start) from error

    def byte_array(self) -> bytes:
        length_offset = self.offset
        length = self.u64()
        if length > MAX_RECORD_BYTES:
            raise CacheDumpError(
                'BYTE_ARRAY_TOO_LARGE', self.stage,
                f'Byte array length {length} exceeds {MAX_RECORD_BYTES}.',
                path=self.path, offset=length_offset)
        return self._take(length)

    def require_end(self) -> None:
        if self.offset != len(self.data):
            raise CacheDumpError(
                'TRAILING_DATA', self.stage,
                f'{len(self.data) - self.offset} trailing bytes remain.',
                path=self.path, offset=self.offset)


def parse_pointer(path: Path) -> dict[str, object]:
    expected = POINTER_KINDS.get(path.name)
    if expected is None:
        raise CacheDumpError(
            'POINTER_NAME', 'pointer', f'Unsupported pointer filename {path.name}.', path=path.name)
    expected_kind, slot = expected
    data = _read_bounded(path, 80, 'pointer')
    if len(data) != 80:
        raise CacheDumpError(
            'POINTER_SIZE', 'pointer', f'Pointer must be exactly 80 bytes, got {len(data)}.',
            path=path.name)
    if data[:8] != POINTER_MAGIC:
        raise CacheDumpError('POINTER_MAGIC', 'pointer', 'Pointer magic is not UEASCV2C.', path=path.name)
    schema = struct.unpack_from('<I', data, 8)[0]
    if schema != 1:
        raise CacheDumpError(
            'POINTER_SCHEMA', 'pointer', f'Unsupported pointer schema {schema}.',
            path=path.name, offset=8)
    if data[12] != expected_kind or data[13:16] != b'\0\0\0':
        raise CacheDumpError(
            'POINTER_KIND', 'pointer', f'Pointer kind does not match {path.name}.',
            path=path.name, offset=12)
    generation_id = data[16:48]
    if generation_id == b'\0' * 32:
        raise CacheDumpError('POINTER_GENERATION', 'pointer', 'GenerationId is zero.', path=path.name, offset=16)
    if blake3_digest(data[:48]) != data[48:80]:
        raise CacheDumpError(
            'POINTER_CHECKSUM', 'pointer', 'Pointer checksum does not match.',
            path=path.name, offset=48)
    return {
        'magic': POINTER_MAGIC.decode('ascii'),
        'slot': slot,
        'pointer_kind_value': expected_kind,
        'schema_version': schema,
        'generation_id': generation_id.hex(),
        'size': len(data),
        'checksum': data[48:80].hex(),
        'checksum_matches': True,
    }


def _known_record_kind(kind: int, stage: str, path: str, offset: int) -> str:
    name = RECORD_KINDS.get(kind)
    if name is None:
        raise CacheDumpError(
            'UNKNOWN_RECORD_KIND', stage, f'Unknown record kind {kind}.',
            path=path, offset=offset)
    return name


def _known_codec(codec: int, stage: str, path: str, offset: int) -> str:
    name = CODECS.get(codec)
    if name is None:
        raise CacheDumpError(
            'UNSUPPORTED_CODEC', stage, f'Unsupported storage codec {codec}.',
            path=path, offset=offset)
    return name


def _decode_raw_record(
    stored: bytes,
    codec_value: int,
    raw_size: int,
    path: str,
    offset: int,
) -> bytes:
    if raw_size > MAX_RECORD_BYTES:
        raise CacheDumpError(
            'RECORD_TOO_LARGE', 'pack-record',
            f'Raw record size {raw_size} exceeds {MAX_RECORD_BYTES}.',
            path=path, offset=offset)
    if codec_value == 0:
        if len(stored) != raw_size:
            raise CacheDumpError(
                'RAW_SIZE_MISMATCH', 'pack-record',
                f'None record stores {len(stored)} bytes but declares {raw_size}.',
                path=path, offset=offset)
        return stored
    if codec_value != 1:
        _known_codec(codec_value, 'pack-record', path, offset)
    try:
        decompressor = zlib.decompressobj()
        raw = decompressor.decompress(stored, raw_size + 1)
    except zlib.error as error:
        raise CacheDumpError(
            'ZLIB_DECODE', 'pack-record', str(error), path=path, offset=offset) from error
    if (len(raw) != raw_size or not decompressor.eof
            or decompressor.unused_data or decompressor.unconsumed_tail):
        raise CacheDumpError(
            'ZLIB_SIZE_MISMATCH', 'pack-record',
            f'Zlib record produced {len(raw)} bytes but declares {raw_size}.',
            path=path, offset=offset)
    return raw


def _opaque_payload(payload: bytes, codec_version: int) -> dict[str, object]:
    return {
        'representation': 'opaque',
        'codec_version': codec_version,
        'size': len(payload),
        'blake3': blake3_digest(payload).hex(),
    }


def _count(reader: Reader, label: str) -> int:
    count = reader.u32()
    if count > MAX_ENTRIES:
        raise CacheDumpError(
            'COUNT_TOO_LARGE', reader.stage,
            f'{label} count {count} exceeds {MAX_ENTRIES}.',
            path=reader.path, offset=reader.offset - 4)
    return count


def _known_enum(
    reader: Reader, value: int, names: dict[int, str], label: str, offset: int,
) -> str:
    name = names.get(value)
    if name is None:
        raise CacheDumpError(
            'UNKNOWN_ENUM_VALUE', reader.stage,
            f'Unknown {label} value {value}.', path=reader.path, offset=offset)
    return name


def _optional_u32(reader: Reader, label: str) -> int | None:
    offset = reader.offset
    tag = reader.u8()
    if tag not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage,
            f'Invalid {label} optional tag {tag}.', path=reader.path, offset=offset)
    return reader.u32() if tag else None


def _optional_string(reader: Reader, label: str) -> str | None:
    offset = reader.offset
    tag = reader.u8()
    if tag not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage,
            f'Invalid {label} optional tag {tag}.', path=reader.path, offset=offset)
    return reader.string() if tag else None


def _stable_reference(reader: Reader) -> dict[str, object]:
    offset = reader.offset
    kind = reader.u8()
    kind_name = _known_enum(
        reader, kind, REFERENCE_KINDS, 'stable reference kind', offset)
    return {
        'kind': kind,
        'kind_name': kind_name,
        'stable_key': reader.hash(),
        'expected_abi': reader.hash(),
    }


def _data_type(reader: Reader, depth: int = 1) -> dict[str, object]:
    if depth > 64:
        raise CacheDumpError(
            'NESTING_DEPTH_EXCEEDED', reader.stage,
            'Data type nesting exceeds 64.', path=reader.path, offset=reader.offset)
    kind_offset = reader.offset
    kind = reader.u8()
    _known_enum(reader, kind, {
        1: 'Primitive', 2: 'ScriptType', 3: 'EnvironmentType', 4: 'Auto',
    }, 'data type kind', kind_offset)
    primitive_offset = reader.offset
    primitive = reader.u8()
    if primitive > 12:
        raise CacheDumpError(
            'UNKNOWN_ENUM_VALUE', reader.stage,
            f'Unknown primitive type {primitive}.', path=reader.path,
            offset=primitive_offset)
    reference_offset = reader.offset
    reference_tag = reader.u8()
    if reference_tag not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage,
            f'Invalid data-type reference tag {reference_tag}.',
            path=reader.path, offset=reference_offset)
    result: dict[str, object] = {
        'kind': kind,
        'primitive': primitive,
    }
    if reference_tag:
        result['type_reference'] = _stable_reference(reader)
    result['qualifier_flags'] = reader.u32()
    result['ordered_sub_types'] = [
        _data_type(reader, depth + 1)
        for _ in range(_count(reader, 'Data type subtype'))
    ]
    return result


def _metadata(reader: Reader) -> dict[str, str]:
    return {'key': reader.string(), 'value': reader.string()}


def _metadata_array(reader: Reader, label: str) -> list[dict[str, str]]:
    return [_metadata(reader) for _ in range(_count(reader, label))]


def _type_dependency(reader: Reader) -> dict[str, object]:
    kind_offset = reader.offset
    kind = reader.u8()
    kind_name = _known_enum(
        reader, kind, DEPENDENCY_KINDS, 'semantic dependency kind', kind_offset)
    result: dict[str, object] = {
        'dependency_kind': kind,
        'dependency_kind_name': kind_name,
        'target': _stable_reference(reader),
    }
    optional_offset = reader.offset
    optional_tag = reader.u8()
    if optional_tag not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage,
            f'Invalid dependency optional tag {optional_tag}.',
            path=reader.path, offset=optional_offset)
    if optional_tag:
        result['expected_content_or_value'] = reader.hash()
    return result


def _declaration_slot(reader: Reader) -> dict[str, object]:
    offset = reader.offset
    kind = reader.u8()
    return {
        'kind': kind,
        'kind_name': _known_enum(
            reader, kind, DECLARATION_SLOT_KINDS,
            'declaration slot kind', offset),
        'ordinal': reader.u32(),
    }


def _parameter(reader: Reader) -> dict[str, object]:
    result = {
        'ordinal': reader.u32(),
        'canonical_name': reader.string(),
        'type': _data_type(reader),
    }
    passing_offset = reader.offset
    passing = reader.u8()
    result['passing'] = passing
    result['passing_name'] = _known_enum(
        reader, passing, PARAMETER_PASSING_KINDS,
        'parameter passing kind', passing_offset)
    result['canonical_default_expression'] = _optional_string(
        reader, 'parameter default expression')
    result['trait_flags'] = reader.u32()
    return result


def _declaration(reader: Reader) -> dict[str, object]:
    kind_offset = reader.offset
    declaration_kind = reader.u8()
    result: dict[str, object] = {
        'declaration_kind': declaration_kind,
        'declaration_kind_name': _known_enum(
            reader, declaration_kind, DECLARATION_KINDS,
            'declaration kind', kind_offset),
        'entity_kind': reader.u8(),
        'schema_coverage': reader.u8(),
        'body_coverage': reader.u8(),
        'stable_key': reader.hash(),
        'owner_kind': reader.u8(),
        'owner_key': reader.hash(),
        'module_key': reader.hash(),
        'canonical_namespace': reader.string(),
        'canonical_name': reader.string(),
        'canonical_declaration': reader.string(),
    }
    result['canonical_identity_traits'] = [
        reader.string() for _ in range(_count(reader, 'Declaration identity trait'))
    ]
    result['canonical_type_spelling'] = _optional_string(
        reader, 'declaration type spelling')
    declared_type_offset = reader.offset
    declared_type_tag = reader.u8()
    if declared_type_tag not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage,
            f'Invalid declared-type optional tag {declared_type_tag}.',
            path=reader.path, offset=declared_type_offset)
    if declared_type_tag:
        result['declared_type'] = _data_type(reader)
    result['ordered_parameters'] = [
        _parameter(reader) for _ in range(_count(reader, 'Declaration parameter'))
    ]
    result.update({
        'trait_flags': reader.u32(),
        'reflection_flags': reader.u32(),
        'metadata': _metadata_array(reader, 'Declaration metadata'),
        'slots': [
            _declaration_slot(reader)
            for _ in range(_count(reader, 'Declaration slot'))
        ],
        'signature_hash': reader.hash(),
        'traits_hash': reader.hash(),
    })
    return result


def _import_declaration(reader: Reader) -> dict[str, object]:
    return {
        'import_key': reader.hash(),
        'canonical_namespace': reader.string(),
        'canonical_name': reader.string(),
        'canonical_signature': reader.string(),
        'target_module_key': reader.hash(),
        'target_declaration': _stable_reference(reader),
        'slots': [
            _declaration_slot(reader)
            for _ in range(_count(reader, 'Import slot'))
        ],
    }


def _summary_source_index(reader: Reader) -> dict[str, object]:
    return {
        'payload_schema_version': reader.u32(),
        'source_snapshot': reader.hash(),
        'decoder_scope': 'common-header-v1',
    }


def _summary_module_interface(reader: Reader) -> dict[str, object]:
    schema = reader.u32()
    module_key = reader.hash()
    module_name = reader.string()
    interface_abi = reader.hash()
    namespace_count = reader.u32()
    if namespace_count > MAX_ENTRIES:
        raise CacheDumpError('COUNT_TOO_LARGE', reader.stage, 'Namespace count exceeds limit.')
    namespaces = [reader.string() for _ in range(namespace_count)]
    declarations = [
        _declaration(reader) for _ in range(_count(reader, 'Declaration'))
    ]
    imports = [
        _import_declaration(reader) for _ in range(_count(reader, 'Import'))
    ]
    dependencies = [
        _type_dependency(reader) for _ in range(_count(reader, 'Interface dependency'))
    ]
    reader.require_end()
    return {
        'payload_schema_version': schema,
        'module_key': module_key,
        'canonical_module_name': module_name,
        'interface_abi': interface_abi,
        'canonical_namespaces': namespaces,
        'declaration_count': len(declarations),
        'declarations': declarations,
        'imports': imports,
        'dependencies': dependencies,
        'decoder_scope': 'module-interface-v1',
    }


def _summary_type_schema(reader: Reader) -> dict[str, object]:
    payload_schema_version = reader.u32()
    result: dict[str, object] = {
        'payload_schema_version': payload_schema_version,
        'module_key': reader.hash(),
        'type_key': reader.hash(),
    }
    type_kind_offset = reader.offset
    type_kind = reader.u8()
    result['type_kind'] = type_kind
    result['type_kind_name'] = _known_enum(
        reader, type_kind, TYPE_KINDS, 'type kind', type_kind_offset)
    result.update({
        'canonical_namespace': reader.string(),
        'canonical_name': reader.string(),
        'canonical_declaration': reader.string(),
        'type_semantic_flags': reader.u32(),
        'metadata': _metadata_array(reader, 'Type metadata'),
    })

    relations = []
    for _ in range(_count(reader, 'Type relation')):
        relation_offset = reader.offset
        relation_kind = reader.u8()
        relation = {
            'kind': relation_kind,
            'kind_name': _known_enum(
                reader, relation_kind, TYPE_RELATION_KINDS,
                'type relation kind', relation_offset),
            'semantic_ordinal': _optional_u32(reader, 'relation ordinal'),
            'target': _stable_reference(reader),
        }
        relations.append(relation)
    result['relations'] = relations

    layout_inputs = []
    for _ in range(_count(reader, 'Type layout input')):
        input_offset = reader.offset
        input_kind = reader.u8()
        layout_inputs.append({
            'kind': input_kind,
            'kind_name': _known_enum(
                reader, input_kind, TYPE_LAYOUT_INPUT_KINDS,
                'type layout input kind', input_offset),
            'target': _stable_reference(reader),
            'boundary_contribution': _optional_u32(
                reader, 'layout boundary contribution'),
            'alignment_contribution': _optional_u32(
                reader, 'layout alignment contribution'),
            'layout_input_hash': reader.hash(),
        })
    result['layout_inputs'] = layout_inputs
    result['layout'] = {
        'semantic_size': reader.u64(),
        'semantic_alignment': reader.u32(),
        'base_property_boundary': reader.u32(),
        'type_layout_hash': reader.hash(),
    }

    properties = []
    for _ in range(_count(reader, 'Ordered property')):
        storage = {
            'layout_ordinal': reader.u32(),
            'semantic_byte_offset': reader.u32(),
            'property_key': reader.hash(),
            'canonical_name': reader.string(),
            'type': _data_type(reader),
        }
        storage_kind_offset = reader.offset
        storage_kind = reader.u8()
        storage['storage_kind'] = storage_kind
        storage['storage_kind_name'] = _known_enum(
            reader, storage_kind, {1: 'InlineValue', 2: 'ObjectHandle'},
            'property storage kind', storage_kind_offset)
        storage.update({
            'semantic_storage_size': reader.u32(),
            'semantic_storage_alignment': reader.u32(),
            'storage_layout_hash': reader.hash(),
        })
        access_offset = reader.offset
        access = reader.u8()
        storage['access'] = access
        storage['access_name'] = _known_enum(
            reader, access, {1: 'Public', 2: 'Protected', 3: 'Private'},
            'member access', access_offset)
        storage.update({
            'property_semantic_flags': reader.u32(),
            'replication_condition': reader.u8(),
            'metadata': _metadata_array(reader, 'Property metadata'),
            'property_layout_fingerprint': reader.hash(),
        })
        properties.append(storage)
    result['ordered_properties'] = properties

    methods = []
    for _ in range(_count(reader, 'Ordered method')):
        kind_offset = reader.offset
        entry_kind = reader.u8()
        methods.append({
            'entry_kind': entry_kind,
            'entry_kind_name': _known_enum(
                reader, entry_kind, METHOD_SLOT_KINDS, 'method slot kind', kind_offset),
            'method_ordinal': reader.u32(),
            'function_key': reader.hash(),
            'declaring_owner': reader.hash(),
            'expected_declaration_abi': reader.hash(),
        })
    result['ordered_methods'] = methods

    virtual_function_table = []
    for _ in range(_count(reader, 'Virtual function table')):
        kind_offset = reader.offset
        slot_kind = reader.u8()
        virtual_function_table.append({
            'slot_kind': slot_kind,
            'slot_kind_name': _known_enum(
                reader, slot_kind, METHOD_SLOT_KINDS, 'VFT slot kind', kind_offset),
            'vft_ordinal': reader.u32(),
            'function_key': reader.hash(),
            'declaring_owner': reader.hash(),
            'implementing_owner': reader.hash(),
            'expected_declaration_abi': reader.hash(),
        })
    result['virtual_function_table'] = virtual_function_table

    behaviors = []
    for _ in range(_count(reader, 'Behavior slot')):
        kind_offset = reader.offset
        behavior_kind = reader.u8()
        behavior = {
            'behavior_kind': behavior_kind,
            'behavior_kind_name': _known_enum(
                reader, behavior_kind, BEHAVIOR_KINDS,
                'behavior kind', kind_offset),
            'slot_ordinal': reader.u32(),
            'target': _stable_reference(reader),
        }
        declaring_owner_offset = reader.offset
        declaring_owner_tag = reader.u8()
        if declaring_owner_tag not in (0, 1):
            raise CacheDumpError(
                'INVALID_OPTIONAL_TAG', reader.stage,
                f'Invalid behavior declaring-owner tag {declaring_owner_tag}.',
                path=reader.path, offset=declaring_owner_offset)
        if declaring_owner_tag:
            behavior['declaring_owner'] = reader.hash()
        behaviors.append(behavior)
    result['ordered_behavior_slots'] = behaviors

    if type_kind == 4:
        enumerators = []
        for _ in range(_count(reader, 'Enum enumerator')):
            enumerators.append({
                'declaration_ordinal': reader.u32(),
                'canonical_name': reader.string(),
                'value': reader.i32(),
                'metadata': _metadata_array(reader, 'Enumerator metadata'),
            })
        result['kind_payload'] = {
            'enumerators': enumerators,
            'enum_authority_hash': reader.hash(),
        }
    elif type_kind in (5, 7):
        multicast_offset = reader.offset + 64
        callable_payload = {
            'signature_function_key': reader.hash(),
            'expected_signature_abi': reader.hash(),
        }
        multicast = reader.u8()
        if multicast not in (0, 1):
            raise CacheDumpError(
                'INVALID_BOOLEAN', reader.stage,
                f'Invalid callable multicast boolean {multicast}.',
                path=reader.path, offset=multicast_offset)
        callable_payload['multicast'] = bool(multicast)
        result['kind_payload'] = callable_payload
    elif type_kind == 6:
        result['kind_payload'] = {'aliased_type': _data_type(reader)}

    reflection_kind_offset = reader.offset
    reflection_kind = reader.u8()
    reflection = {
        'kind': reflection_kind,
        'kind_name': _known_enum(
            reader, reflection_kind, REFLECTION_KINDS,
            'reflection kind', reflection_kind_offset),
        'class_reflection_flags': reader.u32(),
        'config_name': _optional_string(reader, 'reflection config name'),
        'static_class_global_name': _optional_string(
            reader, 'reflection static class global name'),
    }
    reflected_functions = []
    for _ in range(_count(reader, 'Reflected UFunction member')):
        reflected_function = {
            'reflection_ordinal': reader.u32(),
        }
        if payload_schema_version >= 2:
            reflected_function.update({
                'function_name': reader.string(),
                'original_function_name': reader.string(),
                'script_function_name': reader.string(),
            })
        reflected_function['target'] = _stable_reference(reader)
        reflected_functions.append(reflected_function)
    reflection['ordered_ufunction_members'] = reflected_functions
    result['reflection'] = reflection
    result['dependencies'] = [
        _type_dependency(reader) for _ in range(_count(reader, 'Type dependency'))
    ]
    reader.require_end()
    result['decoder_scope'] = f'type-schema-v{payload_schema_version}'
    return result


def _summary_module_state(reader: Reader) -> dict[str, object]:
    return {
        'payload_schema_version': reader.u32(),
        'module_key': reader.hash(),
        'profile': reader.hash(),
        'state_input_hash': reader.hash(),
        'ordered_global_count': reader.u32(),
        'decoder_scope': 'common-header-v1',
        'initializer_payload_policy': 'opaque',
    }


def _read_dependency(reader: Reader) -> dict[str, object]:
    dependency_kind_offset = reader.offset
    dependency_kind = reader.u8()
    if dependency_kind not in DEPENDENCY_KINDS:
        raise CacheDumpError(
            'UNKNOWN_ENUM_VALUE', reader.stage,
            f'Unknown semantic dependency kind {dependency_kind}.',
            path=reader.path, offset=dependency_kind_offset)
    target_kind_offset = reader.offset
    target_kind = reader.u8()
    if target_kind not in REFERENCE_KINDS:
        raise CacheDumpError(
            'UNKNOWN_ENUM_VALUE', reader.stage,
            f'Unknown stable reference kind {target_kind}.',
            path=reader.path, offset=target_kind_offset)
    stable_key = reader.hash()
    expected_abi = reader.hash()
    presence = reader.u8()
    if presence not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage, f'Invalid dependency optional tag {presence}.',
            path=reader.path, offset=reader.offset - 1)
    result: dict[str, object] = {
        'dependency_kind': dependency_kind,
        'dependency_kind_name': DEPENDENCY_KINDS[dependency_kind],
        'target_kind': target_kind,
        'target_kind_name': REFERENCE_KINDS[target_kind],
        'stable_key': stable_key,
        'expected_abi': expected_abi,
    }
    if presence:
        result['expected_content_or_value'] = reader.hash()
    return result


def _summary_function_body(reader: Reader) -> dict[str, object]:
    result: dict[str, object] = {
        'payload_schema_version': reader.u32(),
        'module_key': reader.hash(),
        'function_key': reader.hash(),
        'execution_hash': reader.hash(),
        'debug_hash': reader.hash(),
        'profile': reader.hash(),
        'expected_declaration_abi': reader.hash(),
        'function_source_digest': reader.hash(),
        'function_input_digest': reader.hash(),
        'invocation_kind': reader.u8(),
    }
    vm_codec = reader.u32()
    result['execution_payload'] = _opaque_payload(reader.byte_array(), vm_codec)
    dependency_count = reader.u32()
    if dependency_count > MAX_ENTRIES:
        raise CacheDumpError('COUNT_TOO_LARGE', reader.stage, 'Dependency count exceeds limit.')
    dependencies = [_read_dependency(reader) for _ in range(dependency_count)]
    result['actual_dependencies'] = dependencies
    debug_presence = reader.u8()
    if debug_presence not in (0, 1):
        raise CacheDumpError(
            'INVALID_OPTIONAL_TAG', reader.stage, f'Invalid debug-sidecar tag {debug_presence}.',
            path=reader.path, offset=reader.offset - 1)
    if debug_presence:
        result['debug_sidecar'] = reader.record_id()
    reader.require_end()
    result['decoder_scope'] = 'function-body-v1'
    return result


def _summary_debug_sidecar(reader: Reader) -> dict[str, object]:
    result: dict[str, object] = {
        'payload_schema_version': reader.u32(),
        'function_key': reader.hash(),
        'profile': reader.hash(),
        'debug_hash': reader.hash(),
    }
    codec_version = reader.u32()
    source_count = reader.u32()
    if source_count > MAX_ENTRIES:
        raise CacheDumpError('COUNT_TOO_LARGE', reader.stage, 'Debug source count exceeds limit.')
    sources = []
    for _ in range(source_count):
        sources.append({
            'source_file_key': reader.hash(),
            'logical_section_key': reader.hash(),
            'canonical_logical_section': reader.string(),
        })
    result['sources'] = sources
    result['debug_payload'] = _opaque_payload(reader.byte_array(), codec_version)
    reader.require_end()
    result['decoder_scope'] = 'debug-sidecar-v1'
    return result


def _summary_module_snapshot(reader: Reader) -> dict[str, object]:
    result: dict[str, object] = {
        'payload_schema_version': reader.u32(),
        'module_key': reader.hash(),
    }
    result['module_interface'] = {
        'module_key': reader.hash(),
        **reader.record_id(),
    }
    type_count = reader.u32()
    if type_count > MAX_ENTRIES:
        raise CacheDumpError('COUNT_TOO_LARGE', reader.stage, 'Type link count exceeds limit.')
    result['type_schemas'] = [
        {'type_key': reader.hash(), **reader.record_id()} for _ in range(type_count)
    ]
    result['module_state'] = {
        'module_key': reader.hash(),
        **reader.record_id(),
    }
    function_count = reader.u32()
    if function_count > MAX_ENTRIES:
        raise CacheDumpError('COUNT_TOO_LARGE', reader.stage, 'Function link count exceeds limit.')
    result['function_bodies'] = [
        {'function_key': reader.hash(), **reader.record_id()} for _ in range(function_count)
    ]
    reader.require_end()
    result['decoder_scope'] = 'module-snapshot-v1'
    return result


SUMMARY_DECODERS = {
    1: _summary_source_index,
    2: _summary_module_interface,
    3: _summary_type_schema,
    4: _summary_module_state,
    5: _summary_function_body,
    6: _summary_debug_sidecar,
    7: _summary_module_snapshot,
}

SUMMARY_SCHEMA_VERSIONS = {kind: {1} for kind in SUMMARY_DECODERS}
SUMMARY_SCHEMA_VERSIONS[3] = {1, 2}


def summarize_record(kind: int, payload: bytes, path: str) -> dict[str, object]:
    if len(payload) < 4:
        error = CacheDumpError(
            'OUT_OF_BOUNDS', 'record-payload',
            f'Need 4 bytes at offset 0, file size is {len(payload)}.',
            path=path, offset=0)
        return {
            'decoder_scope': 'unavailable',
            'decoder_error': error.as_dict(),
            'payload_size': len(payload),
            'payload_blake3': blake3_digest(payload).hex(),
        }
    schema_reader = Reader(payload, 'record-payload', path)
    payload_schema_version = schema_reader.u32()
    if payload_schema_version not in SUMMARY_SCHEMA_VERSIONS[kind]:
        error = CacheDumpError(
            'UNSUPPORTED_RECORD_SCHEMA', 'record-payload',
            f'Unsupported {RECORD_KINDS[kind]} payload schema {payload_schema_version}.',
            path=path, offset=0)
        return {
            'decoder_scope': 'unavailable',
            'decoder_error': error.as_dict(),
            'payload_schema_version': payload_schema_version,
            'payload_size': len(payload),
            'payload_blake3': blake3_digest(payload).hex(),
        }
    return SUMMARY_DECODERS[kind](Reader(payload, 'record-payload', path))


def stable_keys_from_summary(summary: dict[str, object]) -> list[str]:
    keys: set[str] = set()
    stable_names = {
        'module_key', 'type_key', 'function_key', 'global_key', 'property_key',
        'stable_key', 'source_file_key', 'logical_section_key',
    }

    def visit(value: object, name: str = '') -> None:
        if isinstance(value, dict):
            for child_name, child in value.items():
                visit(child, child_name)
        elif isinstance(value, list):
            for child in value:
                visit(child, name)
        elif name in stable_names and isinstance(value, str) and len(value) == 64:
            keys.add(value.lower())

    visit(summary)
    return sorted(keys)


def parse_pack(path: Path, expected_pack_id: str | None = None) -> dict[str, object]:
    data = _read_bounded(path, MAX_PACK_BYTES, 'pack')
    display_path = path.name
    if len(data) < PACK_HEADER_SIZE:
        raise CacheDumpError(
            'PACK_TRUNCATED', 'pack', f'Pack is only {len(data)} bytes.', path=display_path)
    if data[:8] != PACK_MAGIC:
        raise CacheDumpError('PACK_MAGIC', 'pack', 'Pack magic is not UEASCV2P.', path=display_path)
    schema, header_size, entry_size, count = struct.unpack_from('<IIII', data, 8)
    data_offset = struct.unpack_from('<Q', data, 24)[0]
    if schema != 1 or header_size != PACK_HEADER_SIZE or entry_size != PACK_INDEX_ENTRY_SIZE:
        raise CacheDumpError(
            'PACK_SCHEMA', 'pack',
            f'Unsupported Pack layout schema={schema} header={header_size} entry={entry_size}.',
            path=display_path, offset=8)
    if count == 0 or count > MAX_ENTRIES:
        raise CacheDumpError(
            'PACK_COUNT', 'pack', f'Invalid Pack record count {count}.',
            path=display_path, offset=20)
    expected_data_offset = PACK_HEADER_SIZE + count * PACK_INDEX_ENTRY_SIZE
    if data_offset != expected_data_offset or data_offset > len(data):
        raise CacheDumpError(
            'PACK_DATA_OFFSET', 'pack',
            f'Pack data offset {data_offset} should be {expected_data_offset}.',
            path=display_path, offset=24)
    computed_pack_id = blake3_digest(data).hex()
    expected = expected_pack_id or (path.stem.lower() if len(path.stem) == 64 else computed_pack_id)
    if computed_pack_id != expected.lower():
        raise CacheDumpError(
            'PACK_ID_MISMATCH', 'pack',
            f'Computed PackId {computed_pack_id} does not match {expected.lower()}.',
            path=display_path)

    records = []
    expected_offset = data_offset
    previous_record_id = ''
    for index in range(count):
        entry_offset = PACK_HEADER_SIZE + index * PACK_INDEX_ENTRY_SIZE
        kind = data[entry_offset]
        codec_value = data[entry_offset + 1]
        kind_name = _known_record_kind(kind, 'pack-index', display_path, entry_offset)
        codec_name = _known_codec(codec_value, 'pack-index', display_path, entry_offset + 1)
        if data[entry_offset + 2:entry_offset + 8] != b'\0' * 6:
            raise CacheDumpError(
                'PACK_RESERVED', 'pack-index', 'Pack index reserved bytes are nonzero.',
                path=display_path, offset=entry_offset + 2)
        content_hash = data[entry_offset + 8:entry_offset + 40].hex()
        pack_offset, stored_size, raw_size = struct.unpack_from('<QQQ', data, entry_offset + 40)
        raw_checksum = data[entry_offset + 64:entry_offset + 96].hex()
        record_id = f'{kind:02x}{content_hash}'
        if previous_record_id and record_id <= previous_record_id:
            raise CacheDumpError(
                'PACK_INDEX_ORDER', 'pack-index', 'Pack record ids are not strictly ordered.',
                path=display_path, offset=entry_offset)
        previous_record_id = record_id
        if pack_offset != expected_offset:
            raise CacheDumpError(
                'PACK_RANGE', 'pack-index',
                f'Record range starts at {pack_offset}; expected {expected_offset}.',
                path=display_path, offset=entry_offset + 40)
        end = pack_offset + stored_size
        if end < pack_offset or end > len(data):
            raise CacheDumpError(
                'PACK_RANGE', 'pack-index',
                f'Record range [{pack_offset}, {end}) exceeds Pack size {len(data)}.',
                path=display_path, offset=entry_offset + 40)
        if stored_size > MAX_RECORD_BYTES or raw_size > MAX_RECORD_BYTES:
            raise CacheDumpError(
                'RECORD_TOO_LARGE', 'pack-index', 'Record size exceeds the Cache V2 limit.',
                path=display_path, offset=entry_offset + 48)
        if codec_value == 0 and stored_size != raw_size:
            raise CacheDumpError(
                'RAW_SIZE_MISMATCH', 'pack-index', 'None codec sizes do not match.',
                path=display_path, offset=entry_offset + 56)
        if codec_value == 1 and (stored_size == 0 or raw_size == 0 or stored_size >= raw_size):
            raise CacheDumpError(
                'ZLIB_SIZE_INVALID', 'pack-index', 'Zlib sizes are not canonical.',
                path=display_path, offset=entry_offset + 48)
        stored = data[pack_offset:end]
        raw = _decode_raw_record(stored, codec_value, raw_size, display_path, pack_offset)
        computed_checksum = blake3_digest(raw).hex()
        if computed_checksum != raw_checksum:
            raise CacheDumpError(
                'RAW_CHECKSUM_MISMATCH', 'pack-record',
                f'Raw checksum {computed_checksum} does not match {raw_checksum}.',
                path=display_path, offset=pack_offset)
        computed_content_hash = record_content_hash(kind, raw).hex()
        if computed_content_hash != content_hash:
            raise CacheDumpError(
                'RECORD_ID_MISMATCH', 'pack-record',
                f'Record content hash {computed_content_hash} does not match {content_hash}.',
                path=display_path, offset=entry_offset + 8)
        summary = summarize_record(kind, raw, display_path)
        records.append({
            'record_id': record_id,
            'record_kind_value': kind,
            'record_kind': kind_name,
            'content_hash': content_hash,
            'codec_value': codec_value,
            'codec': codec_name,
            'pack_offset': pack_offset,
            'stored_size': stored_size,
            'raw_size': raw_size,
            'raw_checksum': raw_checksum,
            'integrity': {
                'raw_checksum_matches': True,
                'record_id_matches': True,
            },
            'stable_keys': stable_keys_from_summary(summary),
            'summary': summary,
        })
        expected_offset = end
    if expected_offset != len(data):
        raise CacheDumpError(
            'PACK_TRAILING_DATA', 'pack', f'{len(data) - expected_offset} trailing bytes.',
            path=display_path, offset=expected_offset)
    return {
        'magic': PACK_MAGIC.decode('ascii'),
        'pack_id': computed_pack_id,
        'schema_version': schema,
        'header_size': header_size,
        'index_entry_size': entry_size,
        'record_count': count,
        'data_offset': data_offset,
        'size': len(data),
        'integrity': {'pack_id_matches': True},
        'records': records,
    }


def parse_manifest(path: Path, expected_generation_id: str | None = None) -> dict[str, object]:
    data = _read_bounded(path, MAX_MANIFEST_BYTES, 'manifest')
    display_path = path.name
    minimum = 181
    if len(data) < minimum:
        raise CacheDumpError(
            'MANIFEST_TRUNCATED', 'manifest', f'Manifest is only {len(data)} bytes.',
            path=display_path)
    reader = Reader(data, 'manifest', display_path)
    if reader._take(8) != MANIFEST_MAGIC:
        raise CacheDumpError('MANIFEST_MAGIC', 'manifest', 'Manifest magic is not UEASCV2M.', path=display_path)
    schema = reader.u32()
    flags = reader.u32()
    if schema != 1 or flags != 0:
        raise CacheDumpError(
            'MANIFEST_SCHEMA', 'manifest', f'Unsupported schema={schema} flags={flags}.',
            path=display_path, offset=8)
    compatibility = reader.hash()
    context = reader.hash()
    profile = reader.hash()
    source_snapshot = reader.hash()
    source_index = reader.record_id()
    if source_index['record_kind_value'] != 1:
        raise CacheDumpError(
            'MANIFEST_SOURCE_INDEX_KIND', 'manifest', 'SourceIndex root has the wrong kind.',
            path=display_path, offset=144)
    module_count_offset = reader.offset
    module_count = reader.u32()
    if module_count > MAX_ENTRIES:
        raise CacheDumpError(
            'MANIFEST_MODULE_COUNT', 'manifest', f'Module count {module_count} exceeds limit.',
            path=display_path, offset=module_count_offset)
    module_roots = []
    previous_module = ''
    for _ in range(module_count):
        module_offset = reader.offset
        module_key = reader.hash()
        record_id = reader.record_id()
        if record_id['record_kind_value'] != 7:
            raise CacheDumpError(
                'MANIFEST_MODULE_ROOT_KIND', 'manifest', 'Module root is not ModuleSnapshot.',
                path=display_path, offset=module_offset + 32)
        if previous_module and module_key <= previous_module:
            raise CacheDumpError(
                'MANIFEST_MODULE_ORDER', 'manifest', 'Module roots are not strictly ordered.',
                path=display_path, offset=module_offset)
        previous_module = module_key
        module_roots.append({'module_key': module_key, **record_id})
    record_count_offset = reader.offset
    record_count = reader.u32()
    if record_count == 0 or record_count > MAX_ENTRIES:
        raise CacheDumpError(
            'MANIFEST_RECORD_COUNT', 'manifest', f'Invalid record count {record_count}.',
            path=display_path, offset=record_count_offset)
    records = []
    previous_record_id = ''
    for _ in range(record_count):
        entry_offset = reader.offset
        record_id = reader.record_id()
        pack_id = reader.hash()
        pack_offset = reader.u64()
        stored_size = reader.u64()
        raw_size = reader.u64()
        codec_value = reader.u8()
        codec = _known_codec(codec_value, 'manifest', display_path, reader.offset - 1)
        raw_checksum = reader.hash()
        if previous_record_id and record_id['record_id'] <= previous_record_id:
            raise CacheDumpError(
                'MANIFEST_RECORD_ORDER', 'manifest', 'Manifest records are not strictly ordered.',
                path=display_path, offset=entry_offset)
        previous_record_id = record_id['record_id']
        records.append({
            **record_id,
            'pack_id': pack_id,
            'pack_offset': pack_offset,
            'stored_size': stored_size,
            'raw_size': raw_size,
            'codec_value': codec_value,
            'codec': codec,
            'raw_checksum': raw_checksum,
            'manifest_entry_offset': entry_offset,
        })
    reader.require_end()
    computed_generation_id = blake3_digest(data).hex()
    expected = expected_generation_id or (
        path.stem.lower() if len(path.stem) == 64 else computed_generation_id)
    if computed_generation_id != expected.lower():
        raise CacheDumpError(
            'GENERATION_ID_MISMATCH', 'manifest',
            f'Computed GenerationId {computed_generation_id} does not match {expected.lower()}.',
            path=display_path)
    return {
        'magic': MANIFEST_MAGIC.decode('ascii'),
        'generation_id': computed_generation_id,
        'schema_version': schema,
        'flags': flags,
        'compatibility': compatibility,
        'context': context,
        'profile': profile,
        'source_snapshot': source_snapshot,
        'source_index': source_index,
        'module_count': module_count,
        'module_roots': module_roots,
        'record_count': record_count,
        'size': len(data),
        'integrity': {'generation_id_matches': True},
        'records': records,
    }


def attach_packs_to_manifest(manifest: dict[str, object], packs_directory: Path) -> dict[str, object]:
    parsed_packs: dict[str, dict[str, object]] = {}
    for entry in manifest['records']:
        pack_id = entry['pack_id']
        if pack_id not in parsed_packs:
            pack_path = packs_directory / f'{pack_id}.aspack'
            if not pack_path.is_file():
                raise CacheDumpError(
                    'PACK_NOT_FOUND', 'manifest-link',
                    f'Manifest references missing Pack {pack_id}.', path=pack_path.name)
            parsed_packs[pack_id] = parse_pack(pack_path, pack_id)

    pack_records = {
        (pack_id, record['record_id']): record
        for pack_id, pack in parsed_packs.items()
        for record in pack['records']
    }
    enriched_records = []
    for entry in manifest['records']:
        pack = parsed_packs[entry['pack_id']]
        end = entry['pack_offset'] + entry['stored_size']
        if end < entry['pack_offset'] or end > pack['size']:
            raise CacheDumpError(
                'MANIFEST_PACK_RANGE', 'manifest-link',
                f"Manifest range [{entry['pack_offset']}, {end}) exceeds Pack size {pack['size']}.",
                path=f"{entry['pack_id']}.aspack", offset=entry['manifest_entry_offset'] + 65)
        record = pack_records.get((entry['pack_id'], entry['record_id']))
        if record is None:
            raise CacheDumpError(
                'MANIFEST_PACK_RECORD', 'manifest-link',
                f"Pack {entry['pack_id']} does not contain record {entry['record_id']}.")
        comparisons = (
            ('pack_offset', entry['pack_offset']),
            ('stored_size', entry['stored_size']),
            ('raw_size', entry['raw_size']),
            ('codec_value', entry['codec_value']),
            ('raw_checksum', entry['raw_checksum']),
        )
        if any(record[name] != value for name, value in comparisons):
            raise CacheDumpError(
                'MANIFEST_PACK_LINK', 'manifest-link',
                f"Manifest location does not match Pack index for {entry['record_id']}.",
                path=f"{entry['pack_id']}.aspack", offset=entry['manifest_entry_offset'])
        enriched_records.append(record)

    result = {key: value for key, value in manifest.items() if key != 'records'}
    result['packs'] = [
        {
            key: value for key, value in parsed_packs[pack_id].items() if key != 'records'
        }
        for pack_id in sorted(parsed_packs)
    ]
    result['records'] = enriched_records
    result['integrity']['manifest_pack_links_match'] = True
    return result
