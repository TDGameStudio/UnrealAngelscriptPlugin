import struct
import sys
from pathlib import Path
import unittest


TOOL_ROOT = Path(__file__).resolve().parents[1]
if str(TOOL_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOL_ROOT))

from cache_v2_format import summarize_record  # noqa: E402


MODULE_KEY = bytes.fromhex('11' * 32)
TYPE_KEY = bytes.fromhex('33' * 32)
BASE_TYPE_KEY = bytes.fromhex('34' * 32)
PROPERTY_KEY = bytes.fromhex('35' * 32)
FUNCTION_KEY = bytes.fromhex('44' * 32)
ABI = bytes.fromhex('aa' * 32)
LAYOUT_HASH = bytes.fromhex('bb' * 32)


def _u32(value: int) -> bytes:
    return struct.pack('<I', value)


def _u64(value: int) -> bytes:
    return struct.pack('<Q', value)


def _string(value: str) -> bytes:
    encoded = value.encode('utf-8')
    return _u32(len(encoded)) + encoded


def _reference(kind: int, stable_key: bytes, abi: bytes = ABI) -> bytes:
    return bytes([kind]) + stable_key + abi


def _primitive_int32() -> bytes:
    return bytes([1, 5, 0]) + _u32(0) + _u32(0)


def _dependency(kind: int, target: bytes) -> bytes:
    return bytes([kind]) + target + bytes([0])


def _type_schema_payload() -> bytes:
    base_reference = _reference(2, BASE_TYPE_KEY)
    function_reference = _reference(3, FUNCTION_KEY)
    return b''.join((
        _u32(2), MODULE_KEY, TYPE_KEY, bytes([1]),
        _string('Game'), _string('PlayerState'),
        _string('class Game::PlayerState'), _u32(0x30),
        _u32(1), _string('Category'), _string('CacheTests'),
        _u32(1), bytes([1]), bytes([0]), base_reference,
        _u32(1), bytes([1]), base_reference, bytes([1]), _u32(16),
        bytes([1]), _u32(8), LAYOUT_HASH,
        _u64(24), _u32(8), _u32(16), LAYOUT_HASH,
        _u32(1), _u32(0), _u32(16), PROPERTY_KEY,
        _string('Health'), _primitive_int32(), bytes([1]), _u32(4),
        _u32(4), LAYOUT_HASH, bytes([1]), _u32(0x3), bytes([0]),
        _u32(1), _string('UProperty'), _string('Health'), LAYOUT_HASH,
        _u32(1), bytes([2]), _u32(0), FUNCTION_KEY, TYPE_KEY, ABI,
        _u32(1), bytes([2]), _u32(0), FUNCTION_KEY, TYPE_KEY, TYPE_KEY, ABI,
        _u32(1), bytes([1]), _u32(0), function_reference,
        bytes([1]), TYPE_KEY,
        # Class has no kind-specific payload.
        bytes([2]), _u32(1), bytes([1]), _string('Game'), bytes([0]),
        _u32(1), _u32(0), _string('Compute'), _string('Compute'),
        _string('Compute_Implementation'), function_reference,
        _u32(1), _dependency(4, base_reference),
    ))


def _module_interface_payload() -> bytes:
    declaration = b''.join((
        bytes([2, 32, 1, 2]), FUNCTION_KEY, bytes([1]), MODULE_KEY,
        MODULE_KEY, _string('Game'), _string('ReadValue'),
        _string('int ReadValue(int Count)'), _u32(0), bytes([0]),
        bytes([1]), _primitive_int32(), _u32(1), _u32(0),
        _string('Count'), _primitive_int32(), bytes([1]), bytes([0]),
        _u32(0), _u32(0), _u32(0), _u32(0), _u32(1),
        bytes([2]), _u32(0), ABI, LAYOUT_HASH,
    ))
    return b''.join((
        _u32(1), MODULE_KEY, _string('Gameplay'), ABI,
        _u32(1), _string('Game'), _u32(1), declaration,
        _u32(0), _u32(0),
    ))


class CacheV2SemanticDecodeTests(unittest.TestCase):
    def test_module_interface_v1_exposes_function_signature_and_slots(self) -> None:
        summary = summarize_record(
            2, _module_interface_payload(), 'module-interface-v1')

        self.assertEqual('module-interface-v1', summary['decoder_scope'])
        self.assertEqual('Gameplay', summary['canonical_module_name'])
        declaration = summary['declarations'][0]
        self.assertEqual(FUNCTION_KEY.hex(), declaration['stable_key'])
        self.assertEqual('ReadValue', declaration['canonical_name'])
        self.assertEqual('Count', declaration['ordered_parameters'][0][
            'canonical_name'])
        self.assertEqual('Function', declaration['slots'][0]['kind_name'])

    def test_type_schema_v2_exposes_layout_inheritance_vft_and_reflection_names(self) -> None:
        summary = summarize_record(3, _type_schema_payload(), 'type-schema-v2')

        self.assertEqual('type-schema-v2', summary['decoder_scope'])
        self.assertEqual('PlayerState', summary['canonical_name'])
        self.assertEqual(BASE_TYPE_KEY.hex(),
                         summary['relations'][0]['target']['stable_key'])
        self.assertEqual('Health', summary['ordered_properties'][0]['canonical_name'])
        self.assertEqual(FUNCTION_KEY.hex(),
                         summary['ordered_methods'][0]['function_key'])
        self.assertEqual(FUNCTION_KEY.hex(),
                         summary['virtual_function_table'][0]['function_key'])
        self.assertEqual('UClass', summary['reflection']['kind_name'])
        self.assertEqual(FUNCTION_KEY.hex(), summary['reflection'][
            'ordered_ufunction_members'][0]['target']['stable_key'])
        self.assertEqual('Compute', summary['reflection'][
            'ordered_ufunction_members'][0]['function_name'])
        self.assertEqual('Compute', summary['reflection'][
            'ordered_ufunction_members'][0]['original_function_name'])
        self.assertEqual('Compute_Implementation', summary['reflection'][
            'ordered_ufunction_members'][0]['script_function_name'])
        self.assertEqual('Inheritance',
                         summary['dependencies'][0]['dependency_kind_name'])


if __name__ == '__main__':
    unittest.main()
