import json
import sys
import tempfile
import unittest
from pathlib import Path


TOOL_ROOT = Path(__file__).resolve().parents[1]
if str(TOOL_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOL_ROOT))

import cache_v2_dump  # noqa: E402


class CacheV2FunctionReuseReportTests(unittest.TestCase):
    def _report(self) -> dict[str, object]:
        return {
            'schemaVersion': 4,
            'mutationPhase': 2,
            'mutationPhaseName': 'RuntimeGameThread',
            'lastTransactionOrdinal': '7',
            'current': {'present': False},
            'pendingColdStart': {'present': False},
            'latestSuccessful': {'present': False},
            'functionRoutes': {'present': False},
            'functionReuse': {
                'schemaVersion': 1,
                'present': True,
                'candidateGenerationId': 'ab' * 32,
                'candidateModuleCount': 3,
                'restoredFunctionCount': 19,
                'compiledMissCount': 4,
                'notCacheableCount': 2,
                'rejectedCorruptCount': 1,
            },
            'decisionTrace': {
                'schemaVersion': 1,
                'enabled': False,
                'capacity': 0,
                'nextEventOrdinal': '0',
                'evictedEventCount': '0',
                'events': [],
            },
        }

    def test_schema4_function_reuse_summary_is_validated_and_rendered(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            report_path = Path(temporary) / 'cache-session-v4.json'
            report_path.write_text(json.dumps(self._report()), encoding='utf-8')
            report = cache_v2_dump._read_session_report(report_path)

        correlation = cache_v2_dump.correlate_session_report(
            {}, report, 'cache-session-v4.json')
        summary = correlation['function_reuse']
        self.assertTrue(summary['present'])
        self.assertEqual('ab' * 32, summary['candidate_generation_id'])
        self.assertFalse(summary['candidate_generation_present_in_store'])
        self.assertEqual(3, summary['candidate_module_count'])
        self.assertEqual(19, summary['restored_function_count'])
        self.assertEqual(4, summary['compiled_miss_count'])
        self.assertEqual(2, summary['not_cacheable_count'])
        self.assertEqual(1, summary['rejected_corrupt_count'])

        text = cache_v2_dump.render_text({
            'ok': True,
            'input_kind': 'namespace',
            'session_correlation': correlation,
        })
        self.assertIn(
            'function-reuse candidate=' + ('ab' * 32), text)
        self.assertIn(
            'modules=3 restored=19 compiled-miss=4 not-cacheable=2 rejected-corrupt=1',
            text)

    def test_schema4_function_reuse_rejects_invalid_stable_coordinates(self) -> None:
        report = self._report()
        report['functionReuse']['candidateGenerationId'] = 'process-pointer'
        with self.assertRaises(cache_v2_dump.CacheDumpError) as caught:
            cache_v2_dump.correlate_session_report(
                {}, report, 'cache-session-v4.json')
        self.assertEqual('SESSION_REPORT_COORDINATE', caught.exception.code)


if __name__ == '__main__':
    unittest.main()
