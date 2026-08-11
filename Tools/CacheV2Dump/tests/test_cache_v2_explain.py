from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from test_cache_v2_dump import (
    FUNCTION_KEY,
    _invoke_main,
    _snapshot_files,
    _write_fixture,
)


class CacheV2DependencyExplainTests(unittest.TestCase):
    def test_function_content_dependency_is_explained_as_a_bounded_cycle(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_fixture(root, function_dependency_kind=12)
            before = _snapshot_files(root)

            exit_code, stdout, stderr = _invoke_main([
                str(root), '--json', '--explain', FUNCTION_KEY.hex(),
            ])

            self.assertEqual(0, exit_code, stderr)
            document = json.loads(stdout)
            self.assertTrue(document['ok'])
            self.assertEqual(1, len(document['dependency_explanations']))
            explanation = document['dependency_explanations'][0]
            self.assertEqual(1, len(explanation['roots']))
            root_node = explanation['roots'][0]
            self.assertEqual('FunctionBody', root_node['record_kind'])
            self.assertEqual(FUNCTION_KEY.hex(), root_node['stable_key'])
            self.assertEqual(1, len(root_node['dependencies']))
            dependency = root_node['dependencies'][0]
            self.assertEqual('FunctionContent', dependency['dependency_kind_name'])
            self.assertEqual('ScriptFunction', dependency['target_kind_name'])
            self.assertEqual(FUNCTION_KEY.hex(), dependency['target_stable_key'])
            self.assertEqual('resolved', dependency['resolution'])
            self.assertEqual('FunctionBody', dependency['target']['record_kind'])
            self.assertTrue(dependency['target']['cycle'])
            self.assertEqual(before, _snapshot_files(root))


if __name__ == '__main__':
    unittest.main()
