#include "check.h"

#include "minecraft/InstallState.h"

using namespace citron;

TEST_CASE(install_stage_transitions) {
    CHECK(canTransition(InstallStage::Idle, InstallStage::Resolving));
    CHECK(canTransition(InstallStage::Resolving, InstallStage::Downloading));
    CHECK(canTransition(InstallStage::Downloading, InstallStage::Verifying));
    CHECK(canTransition(InstallStage::Verifying, InstallStage::Finalizing));
    CHECK(canTransition(InstallStage::Finalizing, InstallStage::Completed));
    CHECK(canTransition(InstallStage::Idle, InstallStage::Deploying));
    CHECK(canTransition(InstallStage::Deploying, InstallStage::Completed));
    CHECK(canTransition(InstallStage::Downloading, InstallStage::Cancelled));
    CHECK(canTransition(InstallStage::Deploying, InstallStage::Failed));
    CHECK(!canTransition(InstallStage::Idle, InstallStage::Completed));
    CHECK(!canTransition(InstallStage::Idle, InstallStage::Failed));
    CHECK(!canTransition(InstallStage::Downloading, InstallStage::Completed));
    CHECK(!canTransition(InstallStage::Completed, InstallStage::Downloading));
    CHECK(canTransition(InstallStage::Failed, InstallStage::Idle));
    CHECK(!canTransition(InstallStage::Verifying, InstallStage::Verifying));
}

TEST_CASE(install_stage_flags) {
    CHECK(isTerminal(InstallStage::Completed));
    CHECK(isTerminal(InstallStage::Failed));
    CHECK(isTerminal(InstallStage::Cancelled));
    CHECK(!isTerminal(InstallStage::Downloading));
    CHECK(isBusy(InstallStage::Downloading));
    CHECK(!isBusy(InstallStage::Idle));
    CHECK(!isBusy(InstallStage::Completed));
}

TEST_CASE(install_progress_fraction) {
    InstallProgress p;
    CHECK_EQ(p.fraction(), 0.0);
    p.total = 200;
    p.done = 50;
    CHECK_EQ(p.fraction(), 0.25);
    p.done = 400;
    CHECK_EQ(p.fraction(), 1.0);
}

TEST_CASE(install_resume_decisions) {
    PartialFile partial;
    partial.size = 1000;
    partial.url = "http://a/x.msixvc";
    partial.totalSize = 5000;
    partial.md5 = "abc";
    CHECK(decideResume(partial, "http://a/x.msixvc", 5000, "ABC") == ResumeDecision::Resume);
    CHECK(decideResume(partial, "http://b/x.msixvc", 5000, "abc") == ResumeDecision::Restart);
    CHECK(decideResume(partial, "http://a/x.msixvc", 6000, "abc") == ResumeDecision::Restart);
    CHECK(decideResume(partial, "http://a/x.msixvc", 5000, "zzz") == ResumeDecision::Restart);
    partial.size = 5000;
    CHECK(decideResume(partial, "http://a/x.msixvc", 5000, "abc") == ResumeDecision::Restart);
    partial.size = 0;
    CHECK(decideResume(partial, "http://a/x.msixvc", 5000, "abc") == ResumeDecision::Restart);
}
