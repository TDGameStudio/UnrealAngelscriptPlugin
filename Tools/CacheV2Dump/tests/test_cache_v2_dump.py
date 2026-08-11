from __future__ import annotations

import contextlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


TOOL_ROOT = Path(__file__).resolve().parents[1]
if str(TOOL_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOL_ROOT))

from cache_v2_dump import (  # noqa: E402
    blake3_digest,
    main,
    record_content_hash,
)
from cache_v2_format import summarize_record  # noqa: E402


KIND_SOURCE_INDEX = 1
KIND_MODULE_INTERFACE = 2
KIND_TYPE_SCHEMA = 3
KIND_MODULE_STATE = 4
KIND_FUNCTION_BODY = 5
KIND_MODULE_SNAPSHOT = 7

MODULE_KEY = bytes.fromhex('11' * 32)
TYPE_KEY = bytes.fromhex('33' * 32)
FUNCTION_KEY = bytes.fromhex('44' * 32)
COMPATIBILITY = bytes.fromhex('55' * 32)
CONTEXT = bytes.fromhex('66' * 32)
PROFILE = bytes.fromhex('77' * 32)
SOURCE_SNAPSHOT = bytes.fromhex('88' * 32)

CPP_EMPTY_PACK_ID = 'cab3a361e1034a5790e3f2d7cfa6b50803f4c7e026a83b95a657e777f48d4e8e'
CPP_EMPTY_PACK = bytes.fromhex(
    '5545415343563250010000002000000060000000010000008000000000000000'
    '01000000000000003dc136fb31bf03a8d55c17197191abbc50c708e405ebedf'
    '10354926365771b54800000000000000000000000000000000000000000000000'
    'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262')
CPP_MINIMUM_GENERATION_ID = 'e4536599616fa82b552de4283e858da39c31d245ac33b829f0b3b9d175269fc4'
CPP_MINIMUM_MANIFEST = bytes.fromhex(
    '554541534356324d0100000000000000c33a5dbefd943e446a91cb9e4f923a2d'
    '75ecf615c3f3f8c88ad89d9f8aaa33af38aadabf648ee783d14a3bc32d76b786'
    'f2647ea2adccb62b3581ff9463768e6a85c25f64159a3ff0d393e11c8bf2f425'
    '79494faa0e8c8a7ab3250e0c5ce1f26b2122232425262728292a2b2c2d2e2f30'
    '3132333435363738393a3b3c3d3e3f40013dc136fb31bf03a8d55c17197191ab'
    'bc50c708e405ebedf10354926365771b540000000001000000013dc136fb31bf'
    '03a8d55c17197191abbc50c708e405ebedf10354926365771b54cab3a361e103'
    '4a5790e3f2d7cfa6b50803f4c7e026a83b95a657e777f48d4e8e800000000000'
    '00000000000000000000000000000000000000af1349b9f5f9a1a6a0404dea36'
    'dcc9499bcb25c9adc112b7cc9a93cae41f3262')
CPP_ZLIB_PACK_ID = 'fe843139ae082b72468153bc5bd5837c4a891978c9e4a234a9f94518983f121b'
CPP_ZLIB_PACK = bytes.fromhex(
    '5545415343563250010000002000000060000000010000008000000000000000'
    '05010000000000001fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcb'
    'c3e47142a3e6328080000000000000000c000000000000004000000000000000'
    'e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c'
    '78da7374a40c0000107e1041')


def _u32(value: int) -> bytes:
    return struct.pack('<I', value)


def _u64(value: int) -> bytes:
    return struct.pack('<Q', value)


def _string(value: str) -> bytes:
    encoded = value.encode('utf-8')
    return _u32(len(encoded)) + encoded


def _byte_array(value: bytes) -> bytes:
    return _u64(len(value)) + value


def _record_id(kind: int, payload: bytes) -> bytes:
    return bytes([kind]) + record_content_hash(kind, payload)


def _source_index_payload() -> bytes:
    return _u32(1) + SOURCE_SNAPSHOT + _u32(7) + _u32(0) + _u32(0)


def _module_interface_payload() -> bytes:
    return b''.join((
        _u32(1), MODULE_KEY, _string('Gameplay'), bytes.fromhex('22' * 32),
        _u32(1), _string('Game'), _u32(0), _u32(0), _u32(0),
    ))


def _type_schema_payload() -> bytes:
    return b''.join((
        _u32(1), MODULE_KEY, TYPE_KEY, bytes([1]), _string('Game'),
        _string('PlayerState'), _string('class Game::PlayerState'), _u32(5),
        _u32(0), _u32(0), _u32(0),
        _u64(0), _u32(1), _u32(0), bytes.fromhex('ab' * 32),
        _u32(0), _u32(0), _u32(0), _u32(0),
        bytes([1]), _u32(0), bytes([0]), bytes([0]), _u32(0),
        _u32(0),
    ))


def _module_state_payload() -> bytes:
    return b''.join((
        _u32(1), MODULE_KEY, PROFILE, bytes.fromhex('99' * 32),
        _u32(0), _u32(0), _u32(0), _u32(0), _u32(0), _u32(0),
    ))


def _function_body_payload(
    dependency_kind: int | None = None,
    schema_version: int = 1,
) -> bytes:
    dependencies = b''
    dependency_count = 0
    if dependency_kind is not None:
        dependency_count = 1
        dependencies = b''.join((
            bytes([dependency_kind, 3]), FUNCTION_KEY,
            bytes.fromhex('a3' * 32), bytes([1]),
            bytes.fromhex('a6' * 32),
        ))
    return b''.join((
        _u32(schema_version), MODULE_KEY, FUNCTION_KEY,
        bytes.fromhex('a1' * 32), bytes.fromhex('a2' * 32), PROFILE,
        bytes.fromhex('a3' * 32), bytes.fromhex('a4' * 32),
        bytes.fromhex('a5' * 32), bytes([2]), _u32(17),
        _byte_array(b'opaque-vm-bytecode'), _u32(dependency_count),
        dependencies, bytes([0]),
    ))


def _module_snapshot_payload(record_ids: dict[int, bytes]) -> bytes:
    return b''.join((
        _u32(1), MODULE_KEY,
        MODULE_KEY, record_ids[KIND_MODULE_INTERFACE],
        _u32(1), TYPE_KEY, record_ids[KIND_TYPE_SCHEMA],
        MODULE_KEY, record_ids[KIND_MODULE_STATE],
        _u32(1), FUNCTION_KEY, record_ids[KIND_FUNCTION_BODY],
    ))


def _build_pack(records: list[tuple[int, bytes, bool]]) -> tuple[bytes, bytes, list[dict[str, object]]]:
    prepared = []
    for kind, payload, compress in records:
        record_id = _record_id(kind, payload)
        stored = zlib.compress(payload, 9) if compress else payload
        codec = 1 if compress else 0
        prepared.append({
            'kind': kind,
            'payload': payload,
            'record_id': record_id,
            'stored': stored,
            'codec': codec,
            'checksum': blake3_digest(payload),
        })
    prepared.sort(key=lambda item: item['record_id'])
    data_offset = 32 + 96 * len(prepared)
    offset = data_offset
    index = bytearray()
    for item in prepared:
        stored = item['stored']
        payload = item['payload']
        index.extend(bytes([item['kind'], item['codec']]))
        index.extend(b'\0' * 6)
        index.extend(item['record_id'][1:])
        index.extend(_u64(offset))
        index.extend(_u64(len(stored)))
        index.extend(_u64(len(payload)))
        index.extend(item['checksum'])
        item['offset'] = offset
        offset += len(stored)
    header = b'UEASCV2P' + _u32(1) + _u32(32) + _u32(96) + _u32(len(prepared)) + _u64(data_offset)
    complete = header + bytes(index) + b''.join(item['stored'] for item in prepared)
    return complete, blake3_digest(complete), prepared


def _build_manifest(
    pack_id: bytes,
    prepared: list[dict[str, object]],
    record_ids: dict[int, bytes],
    first_range_offset: int | None = None,
) -> tuple[bytes, bytes]:
    roots = MODULE_KEY + record_ids[KIND_MODULE_SNAPSHOT]
    locations = bytearray()
    for index, item in enumerate(prepared):
        offset = first_range_offset if index == 0 and first_range_offset is not None else item['offset']
        locations.extend(item['record_id'])
        locations.extend(pack_id)
        locations.extend(_u64(offset))
        locations.extend(_u64(len(item['stored'])))
        locations.extend(_u64(len(item['payload'])))
        locations.extend(bytes([item['codec']]))
        locations.extend(item['checksum'])
    complete = b''.join((
        b'UEASCV2M', _u32(1), _u32(0), COMPATIBILITY, CONTEXT, PROFILE,
        SOURCE_SNAPSHOT, record_ids[KIND_SOURCE_INDEX], _u32(1), roots,
        _u32(len(prepared)), bytes(locations),
    ))
    return complete, blake3_digest(complete)


def _pointer(kind: int, generation_id: bytes) -> bytes:
    prefix = b'UEASCV2C' + _u32(1) + bytes([kind, 0, 0, 0]) + generation_id
    return prefix + blake3_digest(prefix)


def _write_fixture(
    root: Path,
    corrupt_range: bool = False,
    function_dependency_kind: int | None = None,
    function_schema_version: int = 1,
) -> Path:
    payloads = {
        KIND_SOURCE_INDEX: _source_index_payload(),
        KIND_MODULE_INTERFACE: _module_interface_payload(),
        KIND_TYPE_SCHEMA: _type_schema_payload(),
        KIND_MODULE_STATE: _module_state_payload(),
        KIND_FUNCTION_BODY: _function_body_payload(
            function_dependency_kind, function_schema_version),
    }
    record_ids = {kind: _record_id(kind, payload) for kind, payload in payloads.items()}
    payloads[KIND_MODULE_SNAPSHOT] = _module_snapshot_payload(record_ids)
    record_ids[KIND_MODULE_SNAPSHOT] = _record_id(
        KIND_MODULE_SNAPSHOT, payloads[KIND_MODULE_SNAPSHOT])
    pack, pack_id, prepared = _build_pack([
        (kind, payload, kind == KIND_FUNCTION_BODY)
        for kind, payload in payloads.items()
    ])
    manifest, generation_id = _build_manifest(
        pack_id,
        prepared,
        record_ids,
        first_range_offset=len(pack) + 4096 if corrupt_range else None,
    )

    namespace = root / COMPATIBILITY.hex() / CONTEXT.hex()
    packs = namespace / 'Packs'
    generations = namespace / 'Generations'
    packs.mkdir(parents=True)
    generations.mkdir()
    (packs / f'{pack_id.hex()}.aspack').write_bytes(pack)
    (generations / f'{generation_id.hex()}.asmanifest').write_bytes(manifest)
    (namespace / 'Current.ascurrent').write_bytes(_pointer(1, generation_id))
    return namespace


def _snapshot_files(root: Path) -> dict[str, tuple[int, int, bytes]]:
    return {
        path.relative_to(root).as_posix(): (path.stat().st_size, path.stat().st_mtime_ns, path.read_bytes())
        for path in sorted(root.rglob('*')) if path.is_file()
    }


def _invoke_main(arguments: list[str]) -> tuple[int, str, str]:
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        exit_code = main(arguments)
    return exit_code, stdout.getvalue(), stderr.getvalue()


def _json_golden_projection(document: dict[str, object]) -> dict[str, object]:
    namespace = document['namespaces'][0]
    generation = namespace['generations'][0]
    return {
        'format': document['format'],
        'format_version': document['format_version'],
        'input_kind': document['input_kind'],
        'ok': document['ok'],
        'namespace': {
            'compatibility': namespace['compatibility'],
            'context': namespace['context'],
            'pointers': namespace['pointers'],
        },
        'generation': {
            'generation_id': generation['generation_id'],
            'profile': generation['profile'],
            'source_snapshot': generation['source_snapshot'],
            'size': generation['size'],
            'record_count': generation['record_count'],
            'selected_record_count': generation['selected_record_count'],
            'module_roots': generation['module_roots'],
            'packs': generation['packs'],
            'records': [
                {
                    'record_kind': record['record_kind'],
                    'content_hash': record['content_hash'],
                    'codec': record['codec'],
                    'pack_offset': record['pack_offset'],
                    'stored_size': record['stored_size'],
                    'raw_size': record['raw_size'],
                    'stable_keys': record['stable_keys'],
                    'summary': record['summary'],
                }
                for record in generation['records']
            ],
        },
    }


class Blake3Tests(unittest.TestCase):
    def test_known_vectors(self) -> None:
        self.assertEqual(
            'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262',
            blake3_digest(b'').hex(),
        )
        self.assertEqual(
            '6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85',
            blake3_digest(b'abc').hex(),
        )

    def test_multichunk_tree_vector(self) -> None:
        payload = bytes(((index * 37 + 11) & 0xff) for index in range(4097))
        self.assertEqual(
            '183d5ba7adf2556cc7258f746f3042d94b8e9f5a2257ea95abc885499edca3cc',
            blake3_digest(payload).hex(),
        )


class CacheV2DumpTests(unittest.TestCase):
    def test_function_body_byte_array_length_matches_cpp_uint64_contract(self) -> None:
        execution_payload = b'canonical-cpp-vm-payload'
        payload = b''.join((
            _u32(1), MODULE_KEY, FUNCTION_KEY,
            bytes.fromhex('a1' * 32), bytes.fromhex('a2' * 32), PROFILE,
            bytes.fromhex('a3' * 32), bytes.fromhex('a4' * 32),
            bytes.fromhex('a5' * 32), bytes([2]), _u32(17),
            _u64(len(execution_payload)), execution_payload,
            _u32(0), bytes([0]),
        ))

        summary = summarize_record(
            KIND_FUNCTION_BODY, payload, 'cpp-width-function-body')

        self.assertEqual('function-body-v1', summary['decoder_scope'])
        self.assertEqual(len(execution_payload),
                         summary['execution_payload']['size'])
        self.assertEqual([], summary['actual_dependencies'])

    def test_cpp_session_report_correlates_live_publication_to_persisted_generation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            generation_id = next((namespace / 'Generations').glob('*.asmanifest')).stem
            payloads = {
                KIND_SOURCE_INDEX: _source_index_payload(),
                KIND_MODULE_INTERFACE: _module_interface_payload(),
                KIND_TYPE_SCHEMA: _type_schema_payload(),
                KIND_MODULE_STATE: _module_state_payload(),
                KIND_FUNCTION_BODY: _function_body_payload(),
            }
            record_ids = {
                kind: _record_id(kind, payload)
                for kind, payload in payloads.items()
            }
            module_snapshot = _module_snapshot_payload(record_ids)
            module_snapshot_hash = record_content_hash(
                KIND_MODULE_SNAPSHOT, module_snapshot).hex()
            publication = {
                'present': True,
                'publicationSchemaVersion': 2,
                'transactionOrdinal': '7',
                'compileKind': 1,
                'compileKindName': 'Initial',
                'disposition': 1,
                'dispositionName': 'Current',
                'compatibility': COMPATIBILITY.hex(),
                'context': CONTEXT.hex(),
                'profile': PROFILE.hex(),
                'sourceSnapshot': SOURCE_SNAPSHOT.hex(),
                'restoredFromStore': True,
                'persistedGenerationId': generation_id,
                'sourceIndexRecord': {
                    'kind': 1,
                    'kindName': 'SourceIndex',
                    'contentHash': 'aa' * 32,
                },
                'totalRecordCount': 6,
                'canonicalPayloadBytes': '512',
                'modules': [{
                    'moduleKey': MODULE_KEY.hex(),
                    'moduleSnapshotRecord': {
                        'kind': 7,
                        'kindName': 'ModuleSnapshot',
                        'contentHash': module_snapshot_hash,
                    },
                    'totalRecordCount': 6,
                    'canonicalPayloadBytes': '512',
                    'recordKinds': [],
                }],
            }
            session_report = root / 'cache-session.json'
            session_report.write_text(json.dumps({
                'schemaVersion': 2,
                'mutationPhase': 2,
                'mutationPhaseName': 'RuntimeGameThread',
                'lastTransactionOrdinal': '7',
                'current': publication,
                'pendingColdStart': {'present': False},
                'latestSuccessful': publication,
            }), encoding='utf-8')
            before = _snapshot_files(root)

            result = _invoke_main([
                str(root), '--json', '--session-report', str(session_report),
            ])

            self.assertEqual(0, result[0], result[2])
            document = json.loads(result[1])
            correlation = document['session_correlation']
            self.assertEqual(2, correlation['schema_version'])
            self.assertEqual('cache-session.json', correlation['report_name'])
            current = correlation['publications'][0]
            self.assertEqual('Current', current['slot'])
            self.assertEqual('7', current['transaction_ordinal'])
            self.assertEqual(1, len(current['candidates']))
            self.assertTrue(current['candidates'][0]['exact_match'])
            self.assertEqual(generation_id, current['candidates'][0]['generation_id'])
            self.assertEqual([], current['candidates'][0]['mismatches'])
            self.assertEqual(before, _snapshot_files(root))

    def test_schema3_session_report_correlates_module_records_and_function_routes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            generation_id = next(
                (namespace / 'Generations').glob('*.asmanifest')).stem
            payloads = {
                KIND_SOURCE_INDEX: _source_index_payload(),
                KIND_MODULE_INTERFACE: _module_interface_payload(),
                KIND_TYPE_SCHEMA: _type_schema_payload(),
                KIND_MODULE_STATE: _module_state_payload(),
                KIND_FUNCTION_BODY: _function_body_payload(),
            }
            record_ids = {
                kind: _record_id(kind, payload)
                for kind, payload in payloads.items()
            }
            module_snapshot = _module_snapshot_payload(record_ids)
            module_snapshot_hash = record_content_hash(
                KIND_MODULE_SNAPSHOT, module_snapshot).hex()
            publication = {
                'present': True,
                'transactionOrdinal': '11',
                'compatibility': COMPATIBILITY.hex(),
                'context': CONTEXT.hex(),
                'profile': PROFILE.hex(),
                'sourceSnapshot': SOURCE_SNAPSHOT.hex(),
                'modules': [{
                    'moduleKey': MODULE_KEY.hex(),
                    'canonicalModuleName': 'Gameplay',
                    'moduleSnapshotRecord': {
                        'kind': KIND_MODULE_SNAPSHOT,
                        'kindName': 'ModuleSnapshot',
                        'contentHash': module_snapshot_hash,
                    },
                    'types': [{
                        'recordId': {
                            'kind': KIND_TYPE_SCHEMA,
                            'kindName': 'TypeSchema',
                            'contentHash': record_ids[KIND_TYPE_SCHEMA][1:].hex(),
                        },
                        'typeKey': TYPE_KEY.hex(),
                    }],
                    'functions': [{
                        'recordId': {
                            'kind': KIND_FUNCTION_BODY,
                            'kindName': 'FunctionBody',
                            'contentHash': record_ids[KIND_FUNCTION_BODY][1:].hex(),
                        },
                        'functionKey': FUNCTION_KEY.hex(),
                        'executionHash': 'a1' * 32,
                        'debugHash': 'a2' * 32,
                        'profile': PROFILE.hex(),
                    }],
                }],
            }
            report = {
                'schemaVersion': 3,
                'mutationPhase': 2,
                'mutationPhaseName': 'RuntimeGameThread',
                'lastTransactionOrdinal': '11',
                'current': publication,
                'pendingColdStart': {'present': False},
                'latestSuccessful': publication,
                'functionRoutes': {
                    'present': True,
                    'publicationOrdinal': '19',
                    'vmRouteCount': 1,
                    'nativeRouteCount': 0,
                    'routes': [{
                        'moduleKey': MODULE_KEY.hex(),
                        'functionKey': FUNCTION_KEY.hex(),
                        'executionHash': 'a1' * 32,
                        'debugHash': 'a2' * 32,
                        'profile': PROFILE.hex(),
                        'canonicalDeclaration': 'int ReadValue()',
                        'selectedRoute': 1,
                        'selectedRouteName': 'Vm',
                        'verifiedArtifactIdentity': True,
                    }],
                },
            }
            session_report = root / 'cache-session-v3.json'
            session_report.write_text(json.dumps(report), encoding='utf-8')

            result = _invoke_main([
                str(root), '--json', '--session-report', str(session_report),
            ])

            self.assertEqual(0, result[0], result[2])
            correlation = json.loads(result[1])['session_correlation']
            self.assertEqual(3, correlation['schema_version'])
            self.assertTrue(
                correlation['publications'][0]['candidates'][0]['exact_match'])
            route = correlation['function_routes']['routes'][0]
            self.assertEqual(FUNCTION_KEY.hex(), route['function_key'])
            self.assertEqual('Vm', route['selected_route_name'])
            self.assertTrue(route['candidates'][0]['exact_match'])

            report['functionRoutes']['routes'][0]['executionHash'] = 'ff' * 32
            session_report.write_text(json.dumps(report), encoding='utf-8')
            mismatch_result = _invoke_main([
                str(root), '--json', '--session-report', str(session_report),
            ])
            self.assertEqual(0, mismatch_result[0], mismatch_result[2])
            mismatch = json.loads(mismatch_result[1])[
                'session_correlation']['function_routes']['routes'][0][
                'candidates'][0]['mismatches'][0]
            self.assertEqual('executionHash', mismatch['field'])
            self.assertEqual('ff' * 32, mismatch['session'])
            self.assertEqual('a1' * 32, mismatch['store'])

            text_result = _invoke_main([
                str(root), '--session-report', str(session_report),
            ])
            self.assertEqual(0, text_result[0], text_result[2])
            self.assertIn(
                f'function-route Vm {FUNCTION_KEY.hex()}', text_result[1])
            self.assertIn('executionHash session=', text_result[1])
            self.assertIn('store=' + ('a1' * 32), text_result[1])

    def test_cpp_session_report_explains_source_snapshot_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root)
            session_report = root / 'cache-session.json'
            session_report.write_text(json.dumps({
                'schemaVersion': 1,
                'mutationPhase': 2,
                'mutationPhaseName': 'RuntimeGameThread',
                'lastTransactionOrdinal': '9',
                'current': {
                    'present': True,
                    'transactionOrdinal': '9',
                    'compatibility': COMPATIBILITY.hex(),
                    'context': CONTEXT.hex(),
                    'profile': PROFILE.hex(),
                    'sourceSnapshot': 'ff' * 32,
                    'modules': [{'moduleKey': MODULE_KEY.hex()}],
                },
                'pendingColdStart': {'present': False},
                'latestSuccessful': {'present': False},
            }), encoding='utf-8')

            result = _invoke_main([
                str(root), '--json', '--session-report', str(session_report),
            ])

            self.assertEqual(0, result[0], result[2])
            candidate = json.loads(result[1])[
                'session_correlation']['publications'][0]['candidates'][0]
            self.assertFalse(candidate['exact_match'])
            self.assertEqual('sourceSnapshot', candidate['mismatches'][0]['field'])
            self.assertEqual(('ff' * 32), candidate['mismatches'][0]['session'])
            self.assertEqual(SOURCE_SNAPSHOT.hex(), candidate['mismatches'][0]['store'])

    def test_cpp_frozen_pack_manifest_and_zlib_goldens_are_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            namespace = Path(temporary)
            packs = namespace / 'Packs'
            generations = namespace / 'Generations'
            packs.mkdir()
            generations.mkdir()
            empty_pack_path = packs / f'{CPP_EMPTY_PACK_ID}.aspack'
            empty_pack_path.write_bytes(CPP_EMPTY_PACK)
            manifest_path = generations / f'{CPP_MINIMUM_GENERATION_ID}.asmanifest'
            manifest_path.write_bytes(CPP_MINIMUM_MANIFEST)
            zlib_pack_path = packs / f'{CPP_ZLIB_PACK_ID}.aspack'
            zlib_pack_path.write_bytes(CPP_ZLIB_PACK)

            manifest_result = _invoke_main([str(manifest_path), '--json'])
            self.assertEqual(0, manifest_result[0], manifest_result[2])
            generation = json.loads(manifest_result[1])['generations'][0]
            self.assertEqual(CPP_MINIMUM_GENERATION_ID, generation['generation_id'])
            self.assertEqual(CPP_EMPTY_PACK_ID, generation['packs'][0]['pack_id'])
            self.assertTrue(generation['records'][0]['integrity']['record_id_matches'])

            zlib_result = _invoke_main([str(zlib_pack_path), '--json'])
            self.assertEqual(0, zlib_result[0], zlib_result[2])
            record = json.loads(zlib_result[1])['packs'][0]['records'][0]
            self.assertEqual('Zlib', record['codec'])
            self.assertEqual(64, record['raw_size'])
            self.assertTrue(record['integrity']['raw_checksum_matches'])

    def test_valid_root_json_matches_golden_and_is_read_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root)
            before = _snapshot_files(root)
            first = _invoke_main([str(root), '--json'])
            second = _invoke_main([str(root), '--json'])
            self.assertEqual(0, first[0], first[2])
            self.assertEqual(first, second)
            self.assertEqual(before, _snapshot_files(root))
            expected = json.loads(
                (Path(__file__).parent / 'golden' / 'valid_root.json').read_text(encoding='utf-8'))
            self.assertEqual(expected, _json_golden_projection(json.loads(first[1])))

    def test_valid_root_text_matches_golden(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root)
            result = _invoke_main([str(root)])
            self.assertEqual(0, result[0], result[2])
            expected = (Path(__file__).parent / 'golden' / 'valid_root.txt').read_text(encoding='utf-8')
            self.assertEqual(expected, result[1])

    def test_module_kind_and_stable_key_filters_select_one_function(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root)
            result = _invoke_main([
                str(root), '--json', '--module', 'Gameplay',
                '--record-kind', 'FunctionBody', '--stable-key', FUNCTION_KEY.hex(),
            ])
            self.assertEqual(0, result[0], result[2])
            document = json.loads(result[1])
            records = document['namespaces'][0]['generations'][0]['records']
            self.assertEqual(1, len(records))
            self.assertEqual('FunctionBody', records[0]['record_kind'])
            self.assertEqual(FUNCTION_KEY.hex(), records[0]['summary']['function_key'])
            self.assertEqual('opaque', records[0]['summary']['execution_payload']['representation'])

    def test_function_content_dependency_has_symbolic_dump_names(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root, function_dependency_kind=12)
            result = _invoke_main([
                str(root), '--json', '--record-kind', 'FunctionBody',
            ])
            self.assertEqual(0, result[0], result[2])
            record = json.loads(result[1])['namespaces'][0]['generations'][0]['records'][0]
            dependency = record['summary']['actual_dependencies'][0]
            self.assertEqual(12, dependency['dependency_kind'])
            self.assertEqual('FunctionContent', dependency['dependency_kind_name'])
            self.assertEqual(3, dependency['target_kind'])
            self.assertEqual('ScriptFunction', dependency['target_kind_name'])

    def test_unknown_dependency_kind_is_a_structured_decode_error(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root, function_dependency_kind=13)
            before = _snapshot_files(root)
            result = _invoke_main([
                str(root), '--json', '--record-kind', 'FunctionBody',
            ])
            self.assertNotEqual(0, result[0])
            self.assertEqual('', result[2])
            error = json.loads(result[1])['error']
            self.assertEqual('UNKNOWN_ENUM_VALUE', error['code'])
            self.assertEqual('record-payload', error['stage'])
            self.assertEqual(before, _snapshot_files(root))

    def test_unknown_function_body_schema_is_observable_but_not_misdecoded(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root, function_schema_version=2)
            before = _snapshot_files(root)
            result = _invoke_main([
                str(root), '--json', '--record-kind', 'FunctionBody',
            ])
            self.assertEqual(0, result[0], result[2])
            record = json.loads(result[1])['namespaces'][0]['generations'][0]['records'][0]
            self.assertEqual('unavailable', record['summary']['decoder_scope'])
            self.assertEqual('UNSUPPORTED_RECORD_SCHEMA', record['summary']['decoder_error']['code'])
            self.assertEqual(2, record['summary']['payload_schema_version'])
            self.assertEqual(before, _snapshot_files(root))

    def test_generation_id_filter_selects_current_generation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            generation_id = next((namespace / 'Generations').glob('*.asmanifest')).stem
            result = _invoke_main([str(root), '--json', '--generation', generation_id])
            self.assertEqual(0, result[0], result[2])
            document = json.loads(result[1])
            self.assertEqual(generation_id, document['namespaces'][0]['generations'][0]['generation_id'])

    def test_corrupt_manifest_pack_range_is_structured_nonzero_and_read_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root, corrupt_range=True)
            before = _snapshot_files(root)
            result = _invoke_main([str(root), '--json'])
            self.assertNotEqual(0, result[0])
            self.assertEqual('', result[2])
            error = json.loads(result[1])['error']
            self.assertEqual('MANIFEST_PACK_RANGE', error['code'])
            self.assertEqual('manifest-link', error['stage'])
            self.assertEqual(before, _snapshot_files(root))

    def test_pack_direct_and_zlib_payload_integrity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            pack_path = next((namespace / 'Packs').glob('*.aspack'))
            result = _invoke_main([str(pack_path), '--json', '--record-kind', 'FunctionBody'])
            self.assertEqual(0, result[0], result[2])
            document = json.loads(result[1])
            record = document['packs'][0]['records'][0]
            self.assertEqual('Zlib', record['codec'])
            self.assertTrue(record['integrity']['raw_checksum_matches'])
            self.assertTrue(record['integrity']['record_id_matches'])

    def test_corrupt_pack_is_structured_nonzero_and_read_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            pack_path = next((namespace / 'Packs').glob('*.aspack'))
            damaged = bytearray(pack_path.read_bytes())
            damaged[-1] ^= 0x40
            pack_path.write_bytes(damaged)
            before = _snapshot_files(root)
            result = _invoke_main([str(pack_path), '--json'])
            self.assertNotEqual(0, result[0])
            self.assertEqual('', result[2])
            self.assertEqual('PACK_ID_MISMATCH', json.loads(result[1])['error']['code'])
            self.assertEqual(before, _snapshot_files(root))

    def test_invalid_pointer_checksum_is_structured_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            namespace = _write_fixture(root)
            pointer_path = namespace / 'Current.ascurrent'
            damaged = bytearray(pointer_path.read_bytes())
            damaged[-1] ^= 0x80
            pointer_path.write_bytes(damaged)
            result = _invoke_main([str(root), '--json'])
            self.assertNotEqual(0, result[0])
            self.assertEqual('POINTER_CHECKSUM', json.loads(result[1])['error']['code'])

    def test_cli_subprocess_returns_nonzero_without_traceback(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(TOOL_ROOT / 'cache_v2_dump.py'), 'missing.aspack', '--json'],
            cwd=TOOL_ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertNotEqual(0, completed.returncode)
        self.assertEqual('', completed.stderr)
        self.assertNotIn('Traceback', completed.stdout)
        self.assertEqual('INPUT_NOT_FOUND', json.loads(completed.stdout)['error']['code'])

    def test_invalid_filter_is_a_structured_argument_error(self) -> None:
        result = _invoke_main(['unused', '--json', '--record-kind', 'NotARecord'])
        self.assertNotEqual(0, result[0])
        self.assertEqual('', result[2])
        self.assertEqual('ARGUMENT_ERROR', json.loads(result[1])['error']['code'])


if __name__ == '__main__':
    unittest.main()
