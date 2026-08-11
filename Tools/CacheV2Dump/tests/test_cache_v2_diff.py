from __future__ import annotations

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from test_cache_v2_dump import (
    _invoke_main,
    _pointer,
    _snapshot_files,
    _write_fixture,
)


class CacheV2GenerationDiffTests(unittest.TestCase):
    def test_previous_to_current_semantic_diff_joins_changed_function_by_stable_key(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'store'
            changed_root = Path(temporary) / 'changed'
            namespace = _write_fixture(root)
            changed_namespace = _write_fixture(
                changed_root, function_dependency_kind=12)

            previous_generation = next(
                (namespace / 'Generations').glob('*.asmanifest')).stem
            current_generation = next(
                (changed_namespace / 'Generations').glob('*.asmanifest')).stem
            for path in (changed_namespace / 'Packs').glob('*.aspack'):
                shutil.copy2(path, namespace / 'Packs' / path.name)
            for path in (changed_namespace / 'Generations').glob('*.asmanifest'):
                shutil.copy2(path, namespace / 'Generations' / path.name)
            shutil.copy2(
                changed_namespace / 'Current.ascurrent',
                namespace / 'Current.ascurrent')
            (namespace / 'Previous.ascurrent').write_bytes(
                _pointer(2, bytes.fromhex(previous_generation)))
            before = _snapshot_files(root)

            exit_code, stdout, stderr = _invoke_main([
                str(root), '--json', '--diff', 'Previous', 'Current',
            ])

            self.assertEqual(0, exit_code, stderr)
            document = json.loads(stdout)
            self.assertTrue(document['ok'])
            self.assertEqual(1, len(document['generation_diffs']))
            generation_diff = document['generation_diffs'][0]
            self.assertEqual(previous_generation, generation_diff['left_generation_id'])
            self.assertEqual(current_generation, generation_diff['right_generation_id'])
            self.assertEqual({
                'unchanged': 4,
                'changed': 2,
                'added': 0,
                'removed': 0,
            }, generation_diff['counts'])
            changed = [
                entry for entry in generation_diff['entries']
                if entry['status'] == 'changed'
            ]
            self.assertEqual(
                ['FunctionBody', 'ModuleSnapshot'],
                sorted(entry['record_kind'] for entry in changed))
            function_change = next(
                entry for entry in changed
                if entry['record_kind'] == 'FunctionBody')
            self.assertEqual('44' * 32, function_change['stable_key'])
            self.assertNotEqual(
                function_change['left_record_id'],
                function_change['right_record_id'])
            self.assertEqual(before, _snapshot_files(root))


if __name__ == '__main__':
    unittest.main()
