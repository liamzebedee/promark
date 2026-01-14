#include "test_helpers.h"
#include <iostream>

// Exact reproduction of bug-caret-position-after-typing.md:
// 1. Open long-post.md
// 2. Click anywhere in the document to place cursor
// 3. Type some text (e.g., "hello this is a big fat whatever")
// 4. Move caret back two words or so
// 5. Press Enter to insert newline
// 6. Type more text (e.g., "world")
// 7. Press Enter to insert newline
//
// Bug: Caret is in the wrong place at multiple points.

TestResult test_visual_caret_exact_repro(TestContext& ctx) {
    std::vector<std::string> screenshots;
    Engine* engine = ctx.getEngine();

    // Step 1: Load long-post.md content (first few paragraphs)
    std::string content = R"(---
title: "003: Deep love, commitment, letting go"
date: 2025-08-30
---

## Peace and work.

One of the ways to approach life is through a careful selection of what's important, and navigating with ease like an ADHD person dashing at the last second around a kitchen bench, navigating with ease towards that thing.

For myself personally, I notice a deep sense of peace when I occupy myself in some work.)";

    engine->setContent(content);
    engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 0));

    // Step 2: Click in middle of "Peace and work" paragraph (around line 8)
    ctx.simulateClick(150, 180);
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 1));

    std::cout << "  Step 2: Clicked at (150, 180)" << std::endl;

    // Step 3: Type "hello this is big"
    const char* text = "hello this is big";
    for (const char* p = text; *p; p++) {
        if (*p == ' ') {
            ctx.simulateKey(GLFW_KEY_SPACE);
        } else {
            int key = GLFW_KEY_A + (*p - 'a');
            ctx.simulateKey(key);
        }
    }
    for (int i = 0; i < 5; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 2));

    std::cout << "  Step 3: After typing 'hello this is big'" << std::endl;

    // Step 4: Move caret back two words (Alt+Left twice)
    ctx.simulateKey(GLFW_KEY_LEFT, GLFW_MOD_ALT);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 3));

    std::cout << "  Step 4a: After first Alt+Left" << std::endl;

    ctx.simulateKey(GLFW_KEY_LEFT, GLFW_MOD_ALT);
    for (int i = 0; i < 3; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 4));

    std::cout << "  Step 4b: After second Alt+Left" << std::endl;

    // Step 5: Press Enter
    ctx.simulateKey(GLFW_KEY_ENTER);
    for (int i = 0; i < 10; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 5));

    std::cout << "  Step 5: After Enter" << std::endl;

    // Step 6: Type "world"
    const char* world = "world";
    for (const char* p = world; *p; p++) {
        int key = GLFW_KEY_A + (*p - 'a');
        ctx.simulateKey(key);
    }
    for (int i = 0; i < 10; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 6));

    std::cout << "  Step 6: After typing 'world'" << std::endl;

    // Step 7: Press Enter again
    ctx.simulateKey(GLFW_KEY_ENTER);
    for (int i = 0; i < 10; i++) engine->render(ctx.getWidth(), ctx.getHeight());
    screenshots.push_back(ctx.captureScreenshot("visual_caret_bug", 7));

    std::cout << "  Step 7: After second Enter" << std::endl;

    // Output final content for debugging
    std::string finalContent = engine->getContent();
    std::cout << "\n  === FINAL CONTENT (first 500 chars) ===" << std::endl;
    std::cout << finalContent.substr(0, 500) << std::endl;
    std::cout << "  === END ===" << std::endl;

    TEST_PASS();
}

REGISTER_TEST(visual_caret_exact_repro, test_visual_caret_exact_repro);
