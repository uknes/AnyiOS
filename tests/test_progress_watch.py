import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import progress
import progress_watch


class ProgressWatchTests(unittest.TestCase):
    def test_pending_does_not_become_passed(self):
        actual = progress_watch.run_states([
            {"name": "CI", "head_branch": "feature/a",
             "status": "in_progress", "conclusion": None,
             "html_url": "https://github.com/uknes/AnyiOS/actions/runs/1"},
            {"name": "CI", "head_branch": "feature/b",
             "status": "completed", "conclusion": "success"},
        ], "feature/a")
        self.assertEqual(actual[0][1:3], ("in_progress", "pending"))
        self.assertEqual(len(actual), 1)

    def test_counts_and_no_made_up_run(self):
        state = progress.collect(progress.REPO)
        report = progress_watch.render(state,
            [{"number": 11, "html_url": "https://github.com/uknes/AnyiOS/pull/11",
              "draft": False, "head": {"ref": "feature/a"}}],
            [], "2026-10-08 22:00:00")
        self.assertIn(f"| iOS candidate API exports | {state['libraries']['done']} |", report)
        self.assertIn(f"| Compatibility gates | {state['runtime']['done']} |", report)
        self.assertIn("No recent matching CI run", report)
        self.assertIn("neither", report)


if __name__ == "__main__":
    unittest.main()
