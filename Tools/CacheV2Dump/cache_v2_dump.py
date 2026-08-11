#!/usr/bin/env python3
"""Read-only standalone inspector for AngelScript Cache V2 stores."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Iterable

from cache_v2_format import (
    CacheDumpError,
    POINTER_KINDS,
    RECORD_KINDS,
    RECORD_KIND_VALUES,
    attach_packs_to_manifest,
    blake3_digest,
    parse_manifest,
    parse_pack,
    parse_pointer,
    record_content_hash,
)


HEX_PATTERN = re.compile(r'^[0-9a-fA-F]+$')
FORMAT_NAME = 'AngelscriptCacheV2Dump'
FORMAT_VERSION = 1
MAX_SESSION_REPORT_BYTES = 16 * 1024 * 1024
SESSION_PUBLICATION_SLOTS = (
    ('current', 'Current'),
    ('pendingColdStart', 'PendingColdStart'),
    ('latestSuccessful', 'LatestSuccessful'),
)


class StructuredArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise CacheDumpError('ARGUMENT_ERROR', 'arguments', message)


def _kind_value(value: str) -> int:
    lowered = value.lower()
    if lowered in RECORD_KIND_VALUES:
        return RECORD_KIND_VALUES[lowered]
    try:
        numeric = int(value, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            f"Unknown record kind '{value}'. Use {', '.join(RECORD_KINDS.values())}.") from error
    if numeric not in RECORD_KINDS:
        raise argparse.ArgumentTypeError(f'Unknown record kind value {numeric}.')
    return numeric


def _hex_selector(value: str) -> str:
    if not 1 <= len(value) <= 64 or not HEX_PATTERN.fullmatch(value):
        raise argparse.ArgumentTypeError('Stable-key selector must be 1-64 hexadecimal characters.')
    return value.lower()


def _build_parser() -> argparse.ArgumentParser:
    parser = StructuredArgumentParser(
        description='Inspect Cache V2 roots, Manifests, and Packs without launching Unreal.',
    )
    parser.add_argument('input', help='Explicit cache root, namespace root, .asmanifest, or .aspack.')
    parser.add_argument('--json', action='store_true', help='Emit deterministic JSON instead of text.')
    parser.add_argument(
        '--generation', action='append', default=[],
        help='Generation id/prefix or Current, Previous, PendingColdStart (repeatable).')
    parser.add_argument(
        '--module', action='append', default=[],
        help='Exact canonical module name or module stable-key prefix (repeatable).')
    parser.add_argument(
        '--record-kind', action='append', default=[], type=_kind_value,
        help='Record kind name or numeric value (repeatable).')
    parser.add_argument(
        '--stable-key', action='append', default=[], type=_hex_selector,
        help='Stable-key prefix found in the decoded common summary (repeatable).')
    parser.add_argument(
        '--session-report',
        help='Optional Engine-native Cache V2 status JSON to correlate by stable coordinates.')
    parser.add_argument(
        '--diff', nargs=2, metavar=('LEFT', 'RIGHT'),
        help='Semantically diff two generations selected by id/prefix or pointer slot.')
    parser.add_argument(
        '--explain', type=_hex_selector,
        help='Explain persisted dependencies for one stable-key prefix.')
    return parser


def _read_session_report(path: Path) -> dict[str, object]:
    if not path.is_file():
        raise CacheDumpError(
            'SESSION_REPORT_NOT_FOUND', 'session-report',
            'Session report is not a regular file.', path=path.name)
    try:
        size = path.stat().st_size
    except OSError as error:
        raise CacheDumpError(
            'SESSION_REPORT_READ', 'session-report', str(error), path=path.name) from error
    if size > MAX_SESSION_REPORT_BYTES:
        raise CacheDumpError(
            'SESSION_REPORT_TOO_LARGE', 'session-report',
            f'Session report is {size} bytes; limit is {MAX_SESSION_REPORT_BYTES}.',
            path=path.name)
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CacheDumpError(
            'SESSION_REPORT_DECODE', 'session-report', str(error), path=path.name) from error
    if not isinstance(report, dict):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'Session report root must be a JSON object.', path=path.name)
    schema_version = report.get('schemaVersion')
    if schema_version not in (1, 2, 3, 4):
        raise CacheDumpError(
            'SESSION_REPORT_SCHEMA', 'session-report',
            f'Unsupported Engine-native diagnostic schema {schema_version!r}.', path=path.name)
    return report


def _session_hash(
    publication: dict[str, object], field: str, report_name: str
) -> str:
    value = publication.get(field)
    if not isinstance(value, str) or len(value) != 64 or not HEX_PATTERN.fullmatch(value):
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            f'Publication field {field} must be a full 256-bit hexadecimal coordinate.',
            path=report_name)
    return value.lower()


def _document_generations(document: dict[str, object]) -> list[dict[str, object]]:
    generations: list[dict[str, object]] = []
    for namespace in document.get('namespaces', []):
        generations.extend(namespace.get('generations', []))
    generations.extend(document.get('generations', []))
    generations.sort(key=lambda generation: generation['generation_id'])
    return generations


def _session_record_id(
    value: object, field: str, report_name: str, expected_kind: int | None = None,
) -> str:
    if not isinstance(value, dict):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            f'{field} must be a record-id object.', path=report_name)
    kind = value.get('kind')
    content_hash = value.get('contentHash')
    if not isinstance(kind, int) or kind not in RECORD_KINDS:
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            f'{field}.kind must be a known Cache V2 record kind.', path=report_name)
    if expected_kind is not None and kind != expected_kind:
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            f'{field}.kind must be {expected_kind} ({RECORD_KINDS[expected_kind]}).',
            path=report_name)
    if (not isinstance(content_hash, str) or len(content_hash) != 64
            or not HEX_PATTERN.fullmatch(content_hash)):
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            f'{field}.contentHash must be a full 256-bit hexadecimal coordinate.',
            path=report_name)
    return f'{kind:02x}{content_hash.lower()}'


def _session_module_details(
    modules: list[object], report_name: str,
) -> list[dict[str, object]]:
    details = []
    seen_module_keys: set[str] = set()
    for module_index, module in enumerate(modules):
        if not isinstance(module, dict):
            raise CacheDumpError(
                'SESSION_REPORT_SHAPE', 'session-report',
                'Publication modules must contain only objects.', path=report_name)
        module_key = _session_hash(module, 'moduleKey', report_name)
        if module_key in seen_module_keys:
            raise CacheDumpError(
                'SESSION_REPORT_COORDINATE', 'session-report',
                f'Publication contains duplicate moduleKey {module_key}.', path=report_name)
        seen_module_keys.add(module_key)
        detail: dict[str, object] = {
            'module_key': module_key,
            'types': [],
            'functions': [],
        }
        if 'moduleSnapshotRecord' in module:
            detail['module_snapshot_record_id'] = _session_record_id(
                module['moduleSnapshotRecord'],
                f'modules[{module_index}].moduleSnapshotRecord', report_name, 7)
        for array_name, kind, key_name in (
            ('types', 3, 'typeKey'), ('functions', 5, 'functionKey')):
            entries = module.get(array_name, [])
            if not isinstance(entries, list):
                raise CacheDumpError(
                    'SESSION_REPORT_SHAPE', 'session-report',
                    f'modules[{module_index}].{array_name} must be an array.',
                    path=report_name)
            target = detail[array_name]
            assert isinstance(target, list)
            for entry_index, entry in enumerate(entries):
                if not isinstance(entry, dict):
                    raise CacheDumpError(
                        'SESSION_REPORT_SHAPE', 'session-report',
                        f'modules[{module_index}].{array_name}[{entry_index}] must be an object.',
                        path=report_name)
                target.append({
                    'stable_key': _session_hash(entry, key_name, report_name),
                    'record_id': _session_record_id(
                        entry.get('recordId'),
                        f'modules[{module_index}].{array_name}[{entry_index}].recordId',
                        report_name, kind),
                })
        details.append(detail)
    details.sort(key=lambda item: item['module_key'])
    return details


def _generation_record_by_semantic_coordinate(
    generation: dict[str, object], kind: int, stable_key: str,
) -> dict[str, object] | None:
    matches = [
        record for record in generation['records']
        if _semantic_record_coordinate(record) == (kind, stable_key)
    ]
    if len(matches) > 1:
        raise CacheDumpError(
            'SESSION_STORE_DUPLICATE_SEMANTIC_OWNER', 'session-report',
            f'Generation {generation["generation_id"]} has duplicate '
            f'{RECORD_KINDS[kind]} owner {stable_key}.')
    return None if not matches else matches[0]


def _semantic_record_coordinate(record: dict[str, object]) -> tuple[int, str]:
    kind = int(record['record_kind_value'])
    summary = record['summary']
    stable_field = {
        2: 'module_key',
        3: 'type_key',
        4: 'module_key',
        5: 'function_key',
        6: 'function_key',
        7: 'module_key',
    }.get(kind)
    if kind == 1:
        return kind, 'source-index'
    stable_key = summary.get(stable_field, '') if stable_field is not None else ''
    if isinstance(stable_key, str) and len(stable_key) == 64:
        return kind, stable_key.lower()
    # Unknown future payload schemas remain diffable by exact immutable identity,
    # but are not guessed into a stable semantic owner.
    return kind, f"record-id:{record['record_id']}"


def _semantic_record_index(
    generation: dict[str, object],
) -> dict[tuple[int, str], dict[str, object]]:
    index: dict[tuple[int, str], dict[str, object]] = {}
    for record in generation['records']:
        coordinate = _semantic_record_coordinate(record)
        if coordinate in index:
            raise CacheDumpError(
                'DIFF_DUPLICATE_SEMANTIC_OWNER', 'generation-diff',
                f'Generation {generation["generation_id"]} contains duplicate semantic owner '
                f'{record["record_kind"]}:{coordinate[1]}.')
        index[coordinate] = record
    return index


def _loaded_generation_for_selector(
    namespace: dict[str, object], selector: str,
) -> dict[str, object]:
    lowered = selector.lower()
    pointer_map = {
        str(pointer['slot']).lower(): str(pointer['generation_id']).lower()
        for pointer in namespace['pointers']
    }
    target = pointer_map.get(lowered, lowered)
    matches = [
        generation for generation in namespace['generations']
        if str(generation['generation_id']).lower().startswith(target)
    ]
    if not matches:
        raise CacheDumpError(
            'GENERATION_NOT_FOUND', 'generation-diff',
            f'No loaded generation matches diff selector {selector}.')
    if len(matches) > 1:
        raise CacheDumpError(
            'GENERATION_AMBIGUOUS', 'generation-diff',
            f'Diff selector {selector} matches {len(matches)} loaded generations.')
    return matches[0]


def _diff_generation_pair(
    namespace: dict[str, object], left_selector: str, right_selector: str,
) -> dict[str, object]:
    left = _loaded_generation_for_selector(namespace, left_selector)
    right = _loaded_generation_for_selector(namespace, right_selector)
    left_index = _semantic_record_index(left)
    right_index = _semantic_record_index(right)
    entries = []
    counts = {'unchanged': 0, 'changed': 0, 'added': 0, 'removed': 0}
    for coordinate in sorted(set(left_index) | set(right_index)):
        left_record = left_index.get(coordinate)
        right_record = right_index.get(coordinate)
        if left_record is None:
            status = 'added'
        elif right_record is None:
            status = 'removed'
        elif left_record['record_id'] == right_record['record_id']:
            status = 'unchanged'
        else:
            status = 'changed'
        counts[status] += 1
        representative = left_record if left_record is not None else right_record
        assert representative is not None
        stable_key = coordinate[1]
        entries.append({
            'record_kind_value': coordinate[0],
            'record_kind': representative['record_kind'],
            'stable_key': '' if stable_key == 'source-index' else stable_key,
            'semantic_coordinate': f"{representative['record_kind']}:{stable_key}",
            'status': status,
            'left_record_id': None if left_record is None else left_record['record_id'],
            'right_record_id': None if right_record is None else right_record['record_id'],
        })
    return {
        'compatibility': namespace['compatibility'],
        'context': namespace['context'],
        'left_selector': left_selector,
        'right_selector': right_selector,
        'left_generation_id': left['generation_id'],
        'right_generation_id': right['generation_id'],
        'counts': counts,
        'entries': entries,
    }


def build_generation_diffs(
    document: dict[str, object], left_selector: str, right_selector: str,
) -> list[dict[str, object]]:
    namespaces = document.get('namespaces')
    if not isinstance(namespaces, list) or not namespaces:
        raise CacheDumpError(
            'DIFF_REQUIRES_NAMESPACE', 'generation-diff',
            'Generation diff requires a cache or namespace root with pointer slots.')
    return [
        _diff_generation_pair(namespace, left_selector, right_selector)
        for namespace in namespaces
    ]


def _preferred_dependency_target(
    generation: dict[str, object], target_kind: int, stable_key: str,
) -> dict[str, object] | None:
    preferred_kinds = {
        1: (2, 7, 4),
        2: (3,),
        3: (5, 6),
    }.get(target_kind, ())
    candidates = []
    for record in generation['records']:
        coordinate = _semantic_record_coordinate(record)
        if coordinate[1] == stable_key and coordinate[0] in preferred_kinds:
            candidates.append(record)
    candidates.sort(key=lambda record: preferred_kinds.index(record['record_kind_value']))
    return None if not candidates else candidates[0]


def _explain_record_dependencies(
    generation: dict[str, object],
    record: dict[str, object],
    visited_record_ids: frozenset[str],
) -> dict[str, object]:
    coordinate = _semantic_record_coordinate(record)
    node = {
        'record_kind_value': record['record_kind_value'],
        'record_kind': record['record_kind'],
        'record_id': record['record_id'],
        'stable_key': '' if coordinate[1] == 'source-index' else coordinate[1],
        'cycle': record['record_id'] in visited_record_ids,
        'dependencies': [],
    }
    if node['cycle']:
        return node

    next_visited = visited_record_ids | {record['record_id']}
    dependencies = record['summary'].get('actual_dependencies', [])
    ordered_dependencies = sorted(
        dependencies,
        key=lambda dependency: (
            dependency['dependency_kind'], dependency['target_kind'],
            dependency['stable_key'], dependency['expected_abi'],
            dependency.get('expected_content_or_value', ''),
        ),
    )
    for dependency in ordered_dependencies:
        edge = {
            'dependency_kind': dependency['dependency_kind'],
            'dependency_kind_name': dependency['dependency_kind_name'],
            'target_kind': dependency['target_kind'],
            'target_kind_name': dependency['target_kind_name'],
            'target_stable_key': dependency['stable_key'],
            'expected_abi': dependency['expected_abi'],
        }
        if 'expected_content_or_value' in dependency:
            edge['expected_content_or_value'] = dependency['expected_content_or_value']
        target = _preferred_dependency_target(
            generation, dependency['target_kind'], dependency['stable_key'])
        if target is None:
            edge['resolution'] = 'unresolved'
        else:
            edge['resolution'] = 'resolved'
            edge['target'] = _explain_record_dependencies(
                generation, target, next_visited)
        node['dependencies'].append(edge)
    return node


def _explain_generation_dependencies(
    generation: dict[str, object], selector: str,
) -> dict[str, object]:
    matching_by_stable_key: dict[str, list[dict[str, object]]] = {}
    for record in generation['records']:
        coordinate = _semantic_record_coordinate(record)
        if not coordinate[1].startswith('record-id:') and coordinate[1].startswith(selector):
            matching_by_stable_key.setdefault(coordinate[1], []).append(record)
    if not matching_by_stable_key:
        raise CacheDumpError(
            'EXPLAIN_NOT_FOUND', 'dependency-explain',
            f'No semantic record owner matches stable-key prefix {selector}.')

    root_kind_priority = (5, 3, 4, 2, 7, 6, 1)
    roots = []
    for stable_key in sorted(matching_by_stable_key):
        candidates = matching_by_stable_key[stable_key]
        candidates.sort(key=lambda record: (
            root_kind_priority.index(record['record_kind_value'])
            if record['record_kind_value'] in root_kind_priority
            else len(root_kind_priority),
            record['record_id'],
        ))
        roots.append(_explain_record_dependencies(
            generation, candidates[0], frozenset()))
    return {
        'generation_id': generation['generation_id'],
        'selector': selector,
        'roots': roots,
    }


def build_dependency_explanations(
    document: dict[str, object], selector: str,
) -> list[dict[str, object]]:
    generations = _document_generations(document)
    if not generations:
        raise CacheDumpError(
            'EXPLAIN_REQUIRES_GENERATION', 'dependency-explain',
            'Dependency explanation requires at least one selected generation.')
    return [
        _explain_generation_dependencies(generation, selector)
        for generation in generations
    ]


def _correlate_publication(
    slot_name: str,
    publication_value: object,
    generations: list[dict[str, object]],
    report_name: str,
) -> dict[str, object]:
    if not isinstance(publication_value, dict):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            f'Publication slot {slot_name} must be a JSON object.', path=report_name)
    present = publication_value.get('present')
    if not isinstance(present, bool):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            f'Publication slot {slot_name} requires a boolean present field.', path=report_name)
    result: dict[str, object] = {
        'slot': slot_name,
        'present': present,
        'candidates': [],
    }
    if not present:
        return result

    transaction_ordinal = publication_value.get('transactionOrdinal')
    if not isinstance(transaction_ordinal, str) or not transaction_ordinal.isdecimal():
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            f'Publication slot {slot_name} requires a decimal-string transactionOrdinal.',
            path=report_name)
    result['transaction_ordinal'] = transaction_ordinal
    session_coordinates = {
        'compatibility': _session_hash(publication_value, 'compatibility', report_name),
        'context': _session_hash(publication_value, 'context', report_name),
        'profile': _session_hash(publication_value, 'profile', report_name),
        'sourceSnapshot': _session_hash(publication_value, 'sourceSnapshot', report_name),
    }
    modules = publication_value.get('modules')
    if not isinstance(modules, list):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            f'Publication slot {slot_name} requires a modules array.', path=report_name)
    session_modules = _session_module_details(modules, report_name)
    session_module_keys = [
        str(module['module_key']) for module in session_modules]

    candidates = []
    for generation in generations:
        mismatches = []
        store_coordinates = {
            'compatibility': str(generation['compatibility']).lower(),
            'context': str(generation['context']).lower(),
            'profile': str(generation['profile']).lower(),
            'sourceSnapshot': str(generation['source_snapshot']).lower(),
        }
        for field in ('compatibility', 'context', 'profile', 'sourceSnapshot'):
            if session_coordinates[field] != store_coordinates[field]:
                mismatches.append({
                    'field': field,
                    'session': session_coordinates[field],
                    'store': store_coordinates[field],
                })
        store_module_keys = sorted(
            str(root['module_key']).lower() for root in generation['module_roots'])
        if session_module_keys != store_module_keys:
            mismatches.append({
                'field': 'moduleKeys',
                'missingInStore': sorted(set(session_module_keys) - set(store_module_keys)),
                'unexpectedInStore': sorted(set(store_module_keys) - set(session_module_keys)),
            })
        store_roots = {
            str(root['module_key']).lower(): root
            for root in generation['module_roots']
        }
        for session_module in session_modules:
            module_key = str(session_module['module_key'])
            root = store_roots.get(module_key)
            expected_snapshot = session_module.get('module_snapshot_record_id')
            if expected_snapshot is not None and root is not None:
                store_snapshot = str(root['record_id']).lower()
                if expected_snapshot != store_snapshot:
                    mismatches.append({
                        'field': 'moduleSnapshotRecord',
                        'moduleKey': module_key,
                        'session': expected_snapshot,
                        'store': store_snapshot,
                    })
            for array_name, kind in (('types', 3), ('functions', 5)):
                entries = session_module[array_name]
                assert isinstance(entries, list)
                for entry in entries:
                    stable_key = str(entry['stable_key'])
                    store_record = _generation_record_by_semantic_coordinate(
                        generation, kind, stable_key)
                    session_record_id = str(entry['record_id'])
                    if store_record is None:
                        mismatches.append({
                            'field': f'{array_name}Record',
                            'moduleKey': module_key,
                            'stableKey': stable_key,
                            'session': session_record_id,
                            'store': None,
                        })
                    elif session_record_id != str(store_record['record_id']).lower():
                        mismatches.append({
                            'field': f'{array_name}Record',
                            'moduleKey': module_key,
                            'stableKey': stable_key,
                            'session': session_record_id,
                            'store': str(store_record['record_id']).lower(),
                        })
        candidates.append({
            'generation_id': generation['generation_id'],
            'exact_match': not mismatches,
            'mismatches': mismatches,
        })
    result['candidates'] = candidates
    return result


def _correlate_function_routes(
    value: object,
    generations: list[dict[str, object]],
    report_name: str,
) -> dict[str, object]:
    if value is None:
        return {'present': False, 'routes': []}
    if not isinstance(value, dict):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'functionRoutes must be a JSON object.', path=report_name)
    present = value.get('present')
    if not isinstance(present, bool):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'functionRoutes requires a boolean present field.', path=report_name)
    result: dict[str, object] = {'present': present, 'routes': []}
    if not present:
        return result
    publication_ordinal = value.get('publicationOrdinal')
    if not isinstance(publication_ordinal, str) or not publication_ordinal.isdecimal():
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            'functionRoutes.publicationOrdinal must be a decimal string.',
            path=report_name)
    routes = value.get('routes')
    if not isinstance(routes, list):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'functionRoutes.routes must be an array.', path=report_name)
    result.update({
        'publication_ordinal': publication_ordinal,
        'vm_route_count': value.get('vmRouteCount'),
        'native_route_count': value.get('nativeRouteCount'),
    })
    output_routes = []
    for route_index, route in enumerate(routes):
        if not isinstance(route, dict):
            raise CacheDumpError(
                'SESSION_REPORT_SHAPE', 'session-report',
                f'functionRoutes.routes[{route_index}] must be an object.',
                path=report_name)
        module_key = _session_hash(route, 'moduleKey', report_name)
        function_key = _session_hash(route, 'functionKey', report_name)
        route_coordinates = {
            'executionHash': _session_hash(route, 'executionHash', report_name),
            'debugHash': _session_hash(route, 'debugHash', report_name),
            'profile': _session_hash(route, 'profile', report_name),
        }
        route_result = {
            'module_key': module_key,
            'function_key': function_key,
            'canonical_declaration': route.get('canonicalDeclaration', ''),
            'selected_route': route.get('selectedRoute'),
            'selected_route_name': route.get('selectedRouteName', 'Unknown'),
            'verified_artifact_identity': route.get(
                'verifiedArtifactIdentity', False),
            'candidates': [],
        }
        for generation in generations:
            mismatches = []
            store_record = _generation_record_by_semantic_coordinate(
                generation, 5, function_key)
            if store_record is None:
                mismatches.append({
                    'field': 'functionRecord',
                    'session': function_key,
                    'store': None,
                })
            else:
                summary = store_record['summary']
                store_coordinates = {
                    'moduleKey': str(summary.get('module_key', '')).lower(),
                    'executionHash': str(summary.get('execution_hash', '')).lower(),
                    'debugHash': str(summary.get('debug_hash', '')).lower(),
                    'profile': str(summary.get('profile', '')).lower(),
                }
                expected_coordinates = {
                    'moduleKey': module_key,
                    **route_coordinates,
                }
                for field in ('moduleKey', 'executionHash', 'debugHash', 'profile'):
                    if expected_coordinates[field] != store_coordinates[field]:
                        mismatches.append({
                            'field': field,
                            'session': expected_coordinates[field],
                            'store': store_coordinates[field],
                        })
            route_result['candidates'].append({
                'generation_id': generation['generation_id'],
                'exact_match': not mismatches,
                'mismatches': mismatches,
            })
        output_routes.append(route_result)
    output_routes.sort(key=lambda route: route['function_key'])
    result['routes'] = output_routes
    return result


def _correlate_function_reuse(
    value: object,
    generations: list[dict[str, object]],
    report_name: str,
) -> dict[str, object]:
    if value is None:
        return {'present': False}
    if not isinstance(value, dict):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'functionReuse must be a JSON object.', path=report_name)
    present = value.get('present')
    if not isinstance(present, bool):
        raise CacheDumpError(
            'SESSION_REPORT_SHAPE', 'session-report',
            'functionReuse requires a boolean present field.', path=report_name)
    result: dict[str, object] = {'present': present}
    if not present:
        return result
    if value.get('schemaVersion') != 1:
        raise CacheDumpError(
            'SESSION_REPORT_SCHEMA', 'session-report',
            'functionReuse requires summary schemaVersion 1.', path=report_name)
    generation_id = value.get('candidateGenerationId')
    if (not isinstance(generation_id, str) or len(generation_id) != 64
            or not HEX_PATTERN.fullmatch(generation_id)):
        raise CacheDumpError(
            'SESSION_REPORT_COORDINATE', 'session-report',
            'functionReuse.candidateGenerationId must be a full 256-bit hexadecimal coordinate.',
            path=report_name)
    counts: dict[str, int] = {}
    for field in (
        'candidateModuleCount',
        'restoredFunctionCount',
        'compiledMissCount',
        'notCacheableCount',
        'rejectedCorruptCount',
    ):
        count = value.get(field)
        if not isinstance(count, int) or isinstance(count, bool) or count < 0:
            raise CacheDumpError(
                'SESSION_REPORT_SHAPE', 'session-report',
                f'functionReuse.{field} must be a non-negative integer.',
                path=report_name)
        counts[field] = count
    normalized_generation = generation_id.lower()
    result.update({
        'schema_version': 1,
        'candidate_generation_id': normalized_generation,
        'candidate_generation_present_in_store': any(
            str(generation['generation_id']).lower() == normalized_generation
            for generation in generations),
        'candidate_module_count': counts['candidateModuleCount'],
        'restored_function_count': counts['restoredFunctionCount'],
        'compiled_miss_count': counts['compiledMissCount'],
        'not_cacheable_count': counts['notCacheableCount'],
        'rejected_corrupt_count': counts['rejectedCorruptCount'],
    })
    return result


def correlate_session_report(
    document: dict[str, object], report: dict[str, object], report_name: str
) -> dict[str, object]:
    generations = _document_generations(document)
    publications = [
        _correlate_publication(display_name, report.get(field_name), generations, report_name)
        for field_name, display_name in SESSION_PUBLICATION_SLOTS
    ]
    function_routes = _correlate_function_routes(
        report.get('functionRoutes'), generations, report_name)
    function_reuse = _correlate_function_reuse(
        report.get('functionReuse'), generations, report_name)
    return {
        'schema_version': report['schemaVersion'],
        'report_name': report_name,
        'mutation_phase': report.get('mutationPhase'),
        'mutation_phase_name': report.get('mutationPhaseName', 'Unknown'),
        'last_transaction_ordinal': report.get('lastTransactionOrdinal'),
        'publications': publications,
        'function_routes': function_routes,
        'function_reuse': function_reuse,
    }


def _discover_namespaces(root: Path) -> list[Path]:
    if (root / 'Packs').is_dir() and (root / 'Generations').is_dir():
        return [root]
    namespaces = sorted(
        directory.parent
        for directory in root.rglob('Packs')
        if directory.is_dir() and (directory.parent / 'Generations').is_dir()
    )
    unique: list[Path] = []
    seen: set[Path] = set()
    for namespace in namespaces:
        resolved = namespace.resolve()
        if resolved not in seen:
            seen.add(resolved)
            unique.append(namespace)
    if not unique:
        raise CacheDumpError(
            'NAMESPACE_NOT_FOUND', 'input',
            'No directory containing both Packs/ and Generations/ was found.', path=root.name)
    return unique


def _generation_matches(generation_id: str, selectors: Iterable[str]) -> bool:
    lowered = generation_id.lower()
    return any(lowered.startswith(selector.lower()) for selector in selectors)


def _select_generation_ids(
    namespace: Path,
    pointers: list[dict[str, object]],
    selectors: list[str],
) -> list[str]:
    available = sorted(path.stem.lower() for path in (namespace / 'Generations').glob('*.asmanifest'))
    pointer_map = {pointer['slot'].lower(): pointer['generation_id'] for pointer in pointers}
    if not selectors:
        if 'current' in pointer_map:
            return [pointer_map['current']]
        return available
    selected: set[str] = set()
    for selector in selectors:
        lowered = selector.lower()
        if lowered in pointer_map:
            selected.add(pointer_map[lowered])
            continue
        if lowered in ('current', 'previous', 'pendingcoldstart'):
            raise CacheDumpError(
                'GENERATION_SLOT_EMPTY', 'filter', f'Pointer slot {selector} is absent.',
                path=namespace.name)
        matches = [generation for generation in available if generation.startswith(lowered)]
        if not matches:
            raise CacheDumpError(
                'GENERATION_NOT_FOUND', 'filter', f'No generation matches {selector}.',
                path=namespace.name)
        if len(matches) > 1:
            raise CacheDumpError(
                'GENERATION_AMBIGUOUS', 'filter',
                f'Generation prefix {selector} matches {len(matches)} manifests.', path=namespace.name)
        selected.add(matches[0])
    return sorted(selected)


def _record_ids_for_module(
    generation: dict[str, object], selectors: list[str]
) -> set[str] | None:
    if not selectors:
        return None
    records_by_id = {record['record_id']: record for record in generation['records']}
    selected: set[str] = {generation['source_index']['record_id']}
    lowered_selectors = [selector.lower() for selector in selectors]
    for root in generation['module_roots']:
        snapshot = records_by_id.get(root['record_id'])
        if snapshot is None:
            continue
        summary = snapshot['summary']
        interface_link = summary.get('module_interface', {})
        interface = records_by_id.get(interface_link.get('record_id', ''))
        module_name = '' if interface is None else str(
            interface['summary'].get('canonical_module_name', '')).lower()
        module_key = str(root['module_key']).lower()
        matches = any(
            module_name == selector or module_key.startswith(selector)
            for selector in lowered_selectors
        )
        if not matches:
            continue
        selected.add(root['record_id'])
        for link_name in ('module_interface', 'module_state'):
            link = summary.get(link_name, {})
            if 'record_id' in link:
                selected.add(link['record_id'])
        for link_name in ('type_schemas', 'function_bodies'):
            for link in summary.get(link_name, []):
                selected.add(link['record_id'])
        for record_id in list(selected):
            record = records_by_id.get(record_id)
            if record is None:
                continue
            debug = record['summary'].get('debug_sidecar')
            if isinstance(debug, dict) and 'record_id' in debug:
                selected.add(debug['record_id'])
    return selected


def _filter_records(
    generation: dict[str, object],
    modules: list[str],
    kinds: list[int],
    stable_keys: list[str],
) -> None:
    module_record_ids = _record_ids_for_module(generation, modules)
    filtered = []
    for record in generation['records']:
        if module_record_ids is not None and record['record_id'] not in module_record_ids:
            continue
        if kinds and record['record_kind_value'] not in kinds:
            continue
        if stable_keys and not any(
            key.startswith(selector)
            for key in record['stable_keys']
            for selector in stable_keys
        ):
            continue
        filtered.append(record)
    generation['records'] = filtered
    generation['selected_record_count'] = len(filtered)


def _inspect_namespace(
    namespace: Path,
    generation_selectors: list[str],
    modules: list[str],
    kinds: list[int],
    stable_keys: list[str],
) -> dict[str, object]:
    pointers = []
    for filename in POINTER_KINDS:
        path = namespace / filename
        if path.exists():
            if not path.is_file():
                raise CacheDumpError(
                    'POINTER_NOT_FILE', 'pointer', f'{filename} is not a regular file.', path=filename)
            pointers.append(parse_pointer(path))
    pointers.sort(key=lambda pointer: pointer['pointer_kind_value'])
    generation_ids = _select_generation_ids(namespace, pointers, generation_selectors)
    generations = []
    for generation_id in generation_ids:
        manifest_path = namespace / 'Generations' / f'{generation_id}.asmanifest'
        if not manifest_path.is_file():
            raise CacheDumpError(
                'MANIFEST_NOT_FOUND', 'manifest',
                f'Pointer references missing Manifest {generation_id}.', path=manifest_path.name)
        manifest = parse_manifest(manifest_path, generation_id)
        generation = attach_packs_to_manifest(manifest, namespace / 'Packs')
        _filter_records(generation, modules, kinds, stable_keys)
        generations.append(generation)
    compatibility = namespace.parent.name.lower()
    context = namespace.name.lower()
    if generations:
        compatibility = generations[0]['compatibility']
        context = generations[0]['context']
    return {
        'compatibility': compatibility,
        'context': context,
        'pointers': pointers,
        'generations': generations,
    }


def inspect(
    input_path: Path,
    generation_selectors: list[str],
    modules: list[str],
    kinds: list[int],
    stable_keys: list[str],
) -> dict[str, object]:
    if not input_path.exists():
        raise CacheDumpError(
            'INPUT_NOT_FOUND', 'input', f'Input does not exist: {input_path}.', path=input_path.name)
    if input_path.is_file():
        suffix = input_path.suffix.lower()
        if suffix == '.aspack':
            pack = parse_pack(input_path)
            pack['records'] = [
                record for record in pack['records']
                if (not kinds or record['record_kind_value'] in kinds)
                and (not stable_keys or any(
                    key.startswith(selector)
                    for key in record['stable_keys'] for selector in stable_keys))
            ]
            if modules:
                lowered_modules = [value.lower() for value in modules]
                pack['records'] = [
                    record for record in pack['records']
                    if any(
                        key.startswith(selector)
                        for key in record['stable_keys'] for selector in lowered_modules
                    )
                    or str(record['summary'].get('canonical_module_name', '')).lower()
                    in lowered_modules
                ]
            pack['selected_record_count'] = len(pack['records'])
            return {
                'format': FORMAT_NAME,
                'format_version': FORMAT_VERSION,
                'ok': True,
                'input_kind': 'pack',
                'packs': [pack],
            }
        if suffix == '.asmanifest':
            generation_id = input_path.stem.lower()
            manifest = parse_manifest(input_path, generation_id if len(generation_id) == 64 else None)
            if generation_selectors and not _generation_matches(
                manifest['generation_id'], generation_selectors):
                raise CacheDumpError(
                    'GENERATION_NOT_FOUND', 'filter', 'Explicit Manifest does not match the generation filter.',
                    path=input_path.name)
            generation = attach_packs_to_manifest(manifest, input_path.parent.parent / 'Packs')
            _filter_records(generation, modules, kinds, stable_keys)
            return {
                'format': FORMAT_NAME,
                'format_version': FORMAT_VERSION,
                'ok': True,
                'input_kind': 'manifest',
                'generations': [generation],
            }
        if input_path.name in POINTER_KINDS:
            namespace = input_path.parent
            slot = POINTER_KINDS[input_path.name][1]
            namespace_result = _inspect_namespace(
                namespace, generation_selectors or [slot], modules, kinds, stable_keys)
            return {
                'format': FORMAT_NAME,
                'format_version': FORMAT_VERSION,
                'ok': True,
                'input_kind': 'pointer',
                'namespaces': [namespace_result],
            }
        raise CacheDumpError(
            'INPUT_KIND', 'input',
            'Input file must be .aspack, .asmanifest, or a Cache V2 pointer.', path=input_path.name)
    if not input_path.is_dir():
        raise CacheDumpError('INPUT_KIND', 'input', 'Input is not a regular file or directory.', path=input_path.name)
    namespaces = [
        _inspect_namespace(namespace, generation_selectors, modules, kinds, stable_keys)
        for namespace in _discover_namespaces(input_path)
    ]
    namespaces.sort(key=lambda item: (item['compatibility'], item['context']))
    return {
        'format': FORMAT_NAME,
        'format_version': FORMAT_VERSION,
        'ok': True,
        'input_kind': 'cache-root',
        'namespaces': namespaces,
    }


def _text_generation(lines: list[str], generation: dict[str, object], indent: str) -> None:
    lines.append(f"{indent}generation {generation['generation_id']}")
    lines.append(
        f"{indent}  manifest schema={generation['schema_version']} size={generation['size']} "
        f"records={generation['record_count']} selected={generation.get('selected_record_count', generation['record_count'])}")
    lines.append(f"{indent}  profile {generation['profile']}")
    lines.append(f"{indent}  source-snapshot {generation['source_snapshot']}")
    lines.append(f"{indent}  module-roots {generation['module_count']}")
    for root in generation['module_roots']:
        lines.append(
            f"{indent}    {root['module_key']} -> {root['record_kind']}:{root['content_hash']}")
    lines.append(f"{indent}  packs {len(generation['packs'])}")
    for pack in generation['packs']:
        lines.append(
            f"{indent}    {pack['pack_id']} size={pack['size']} records={pack['record_count']}")
    lines.append(f"{indent}  records {len(generation['records'])}")
    for record in generation['records']:
        lines.append(
            f"{indent}    {record['record_kind']} {record['content_hash']} "
            f"codec={record['codec']} stored={record['stored_size']} raw={record['raw_size']} "
            f"offset={record['pack_offset']}")
        for key in record['stable_keys']:
            lines.append(f'{indent}      stable-key {key}')


def render_text(document: dict[str, object]) -> str:
    if not document.get('ok'):
        error = document['error']
        location = ''
        if 'path' in error:
            location += f" path={error['path']}"
        if 'offset' in error:
            location += f" offset={error['offset']}"
        return (
            f"AngelScript Cache V2 Dump ERROR\n"
            f"code: {error['code']}\n"
            f"stage: {error['stage']}\n"
            f"message: {error['message']}{location}\n"
        )
    lines = [
        'AngelScript Cache V2 Dump',
        f"input-kind: {document['input_kind']}",
    ]
    for namespace in document.get('namespaces', []):
        lines.append(f"namespace {namespace['compatibility']}/{namespace['context']}")
        lines.append(f"  pointers {len(namespace['pointers'])}")
        for pointer in namespace['pointers']:
            lines.append(f"    {pointer['slot']} -> {pointer['generation_id']}")
        for generation in namespace['generations']:
            _text_generation(lines, generation, '  ')
    for generation in document.get('generations', []):
        _text_generation(lines, generation, '')
    for pack in document.get('packs', []):
        lines.append(f"pack {pack['pack_id']} size={pack['size']} records={pack['record_count']}")
        for record in pack['records']:
            lines.append(
                f"  {record['record_kind']} {record['content_hash']} codec={record['codec']} "
                f"stored={record['stored_size']} raw={record['raw_size']} offset={record['pack_offset']}")
    correlation = document.get('session_correlation')
    if correlation is not None:
        lines.append(
            f"session-report {correlation['report_name']} "
            f"schema={correlation['schema_version']} "
            f"phase={correlation['mutation_phase_name']} "
            f"last-transaction={correlation['last_transaction_ordinal']}")
        for publication in correlation['publications']:
            if not publication['present']:
                lines.append(f"  {publication['slot']} absent")
                continue
            exact_count = sum(
                1 for candidate in publication['candidates'] if candidate['exact_match'])
            lines.append(
                f"  {publication['slot']} transaction={publication['transaction_ordinal']} "
                f"candidates={len(publication['candidates'])} exact={exact_count}")
            for candidate in publication['candidates']:
                status = 'exact' if candidate['exact_match'] else 'mismatch'
                lines.append(f"    {candidate['generation_id']} {status}")
                for mismatch in candidate['mismatches']:
                    detail = ''
                    if 'session' in mismatch or 'store' in mismatch:
                        detail = (
                            f" session={mismatch.get('session')}"
                            f" store={mismatch.get('store')}")
                    lines.append(f"      {mismatch['field']}{detail}")
        function_routes = correlation.get('function_routes', {'present': False})
        if function_routes.get('present'):
            lines.append(
                f"  function-routes publication={function_routes['publication_ordinal']} "
                f"vm={function_routes.get('vm_route_count')} "
                f"native={function_routes.get('native_route_count')}")
            for route in function_routes['routes']:
                exact_count = sum(
                    1 for candidate in route['candidates']
                    if candidate['exact_match'])
                lines.append(
                    f"    function-route {route['selected_route_name']} "
                    f"{route['function_key']} verified="
                    f"{str(route['verified_artifact_identity']).lower()} "
                    f"exact={exact_count}/{len(route['candidates'])}")
                for candidate in route['candidates']:
                    status = 'exact' if candidate['exact_match'] else 'mismatch'
                    lines.append(
                        f"      {candidate['generation_id']} {status}")
                    for mismatch in candidate['mismatches']:
                        lines.append(
                            f"        {mismatch['field']} "
                            f"session={mismatch.get('session')} "
                            f"store={mismatch.get('store')}")
        function_reuse = correlation.get('function_reuse', {'present': False})
        if function_reuse.get('present'):
            lines.append(
                f"  function-reuse candidate={function_reuse['candidate_generation_id']} "
                f"store-present={str(function_reuse['candidate_generation_present_in_store']).lower()}")
            lines.append(
                f"    modules={function_reuse['candidate_module_count']} "
                f"restored={function_reuse['restored_function_count']} "
                f"compiled-miss={function_reuse['compiled_miss_count']} "
                f"not-cacheable={function_reuse['not_cacheable_count']} "
                f"rejected-corrupt={function_reuse['rejected_corrupt_count']}")
    for generation_diff in document.get('generation_diffs', []):
        counts = generation_diff['counts']
        lines.append(
            f"generation-diff {generation_diff['left_generation_id']} -> "
            f"{generation_diff['right_generation_id']} unchanged={counts['unchanged']} "
            f"changed={counts['changed']} added={counts['added']} removed={counts['removed']}")
        for entry in generation_diff['entries']:
            lines.append(
                f"  {entry['status']} {entry['semantic_coordinate']} "
                f"left={entry['left_record_id']} right={entry['right_record_id']}")
    for explanation in document.get('dependency_explanations', []):
        lines.append(
            f"dependency-explain generation={explanation['generation_id']} "
            f"selector={explanation['selector']} roots={len(explanation['roots'])}")
        for root in explanation['roots']:
            lines.append(
                f"  {root['record_kind']} {root['stable_key']} "
                f"dependencies={len(root['dependencies'])}")
            for dependency in root['dependencies']:
                lines.append(
                    f"    {dependency['dependency_kind_name']} -> "
                    f"{dependency['target_kind_name']}:{dependency['target_stable_key']} "
                    f"{dependency['resolution']}")
    return '\n'.join(lines) + '\n'


def _json_text(document: dict[str, object]) -> str:
    return json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + '\n'


def main(arguments: list[str] | None = None) -> int:
    parser = _build_parser()
    try:
        options = parser.parse_args(arguments)
        if options.diff and options.generation:
            raise CacheDumpError(
                'ARGUMENT_ERROR', 'arguments',
                '--diff selects both generations and cannot be combined with --generation.')
        generation_selectors = options.diff if options.diff else options.generation
        document = inspect(
            Path(options.input),
            generation_selectors,
            options.module,
            options.record_kind,
            options.stable_key,
        )
        if options.diff:
            document['generation_diffs'] = build_generation_diffs(
                document, options.diff[0], options.diff[1])
        if options.explain:
            document['dependency_explanations'] = build_dependency_explanations(
                document, options.explain)
        if options.session_report:
            session_report_path = Path(options.session_report)
            session_report = _read_session_report(session_report_path)
            document['session_correlation'] = correlate_session_report(
                document, session_report, session_report_path.name)
        output = _json_text(document) if options.json else render_text(document)
        sys.stdout.write(output)
        return 0
    except CacheDumpError as error:
        document = {
            'format': FORMAT_NAME,
            'format_version': FORMAT_VERSION,
            'ok': False,
            'error': error.as_dict(),
        }
        use_json = '--json' in (arguments if arguments is not None else sys.argv[1:])
        sys.stdout.write(_json_text(document) if use_json else render_text(document))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
