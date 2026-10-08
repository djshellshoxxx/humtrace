# HumTrace GUI toolkit evaluation

**Reviewed:** 2026-10-08
**Scope:** Windows and Linux standalone v0.1 UI. This is a toolkit comparison, not a selection or a licensing decision. Upstream support means the toolkit documents or implements a target; HumTrace must still build, test, package, and verify its own app natively on both systems.

## Comparison

| Option | Windows/Linux platform and build path | Accessibility | Testing | License and unresolved tradeoff |
|---|---|---|---|---|
| **JUCE** | Upstream lists Windows and Linux among its desktop targets. Include JUCE sources through CMake or use Projucer. Strong fit for an audio application: it supplies widgets, graphics, audio file support, audio device handling, and DSP. [1][2] | JUCE says its controls connect to native accessibility controls by default. Custom controls such as a spectrogram or spectrum plot still need verification and possibly added accessibility metadata. [1] | Provides `juce::UnitTest` and `UnitTestRunner`; the app can keep its analysis and report tests in a separate native test target. [3] | JUCE 9 modules are dual licensed under AGPLv3 and JUCE's commercial terms. Its current commercial EULA also defines tiers, revenue/funding limits, subscriptions/perpetual licenses, and seats. The applicable path depends on HumTrace's product license, distribution, and developer situation; review the current terms before adopting. [4][5] |
| **Qt 6 Widgets** | Qt 6.12's supported desktop matrix includes Windows 10/11 and specified Linux distributions. Qt recommends CMake for new work; modules are found and linked by component. [6][7] | Qt documents platform accessibility bridges for assistive tools, including screen readers and braille displays. It names MSAA, macOS Accessibility, and Unix/X11 AT-SPI support. Built in widgets expose useful metadata; custom widgets can implement `QAccessibleInterface`. [8][9] | Qt Test is a unit testing framework for Qt applications and libraries and integrates with CMake/CTest. It supports test fixtures, data driven tests, and report output. [10][11] | Qt offers commercial licensing and open source licensing. Qt 6.12's licensing page identifies LGPLv3 for eligible modules, GPLv3 for some listed modules, and commercial terms; module selection matters. Decide whether the intended product can meet the applicable open source license terms or whether a commercial license is required. [12] |
| **wxWidgets** | wxWidgets supports Windows, Linux and macOS from a common C++ API and aims to use platform native controls. CMake builds and example CMake projects are available; platform and compiler combinations still need to be checked for the selected stable release. [13][14] | Native controls may integrate with the platform's assistive technology. wxWidgets also has `wxAccessible` for supplying names, roles, values, focus, and actions. The documented `wxAccessible` implementation is MSAA on Windows; do not assume equivalent custom widget accessibility on Linux. [15][16] | wxWidgets maintains automated builds and a project test suite, but this is primarily testing wxWidgets itself. HumTrace would use its preferred test framework for product behavior and add GUI automation or manual assistive technology checks. [17] | The wxWindows Library Licence is LGPL-based with an exception allowing static or dynamic linking and distribution of application binaries under the developer's own terms. Individual files may have different terms, so check the exact release and copied files. This is a permissive distribution path with notice and source-code handling details to review. [18][19] |
| **SDL2 + Dear ImGui** | SDL2 supplies windowing, input, and platform integration; Dear ImGui supplies the UI. Its official SDL2 platform backend lists Windows, macOS and Linux. Dear ImGui expects one platform backend and one renderer backend; the developer chooses and packages both. [20][21] | Dear ImGui is a custom rendered widget system. Its backend documentation describes input, cursor, clipboard and rendering integration, but does not document screen reader semantics. The upstream issue tracker contains accessibility requests; treat keyboard and assistive technology support as a design and validation task. This is the largest known accessibility uncertainty for HumTrace. [21][22] | SDL2 includes its own test support, which is aimed at SDL and can assist with low level integration. Dear ImGui's separate Test Engine supports UI automation; its engine code has a distinct license from the MIT licensed core and its free/paid eligibility conditions must be checked. A conventional unit test framework can still test HumTrace logic. [23][24][25] | SDL2 uses the zlib license and Dear ImGui uses MIT. These are short permissive licenses, but retain the notices. Dear ImGui Test Engine is a separate product license; using it for internal GUI automation may introduce a distinct business eligibility or paid license question. [24][25] |

## What matters for HumTrace

HumTrace's planned UI includes file selection, channel and analysis controls, a waveform, spectrum, spectrogram, finding timeline, and reproducible report export. The charts are custom visual controls in every option. The main toolkit distinction is how much infrastructure comes ready: JUCE offers audio focused facilities, Qt offers a broad general application framework with documented accessibility and testing support, wxWidgets emphasizes native controls, while SDL2 plus Dear ImGui gives a compact custom drawn interface and leaves more accessibility and application framework work to HumTrace.

The least settled release risk is accessibility for the custom plots and timeline, especially keyboard navigation, readable text alternatives for findings, screen reader exposure, high contrast behavior, scaling, and color independent cues. Each candidate needs a focused Windows and Linux prototype: navigate the complete analysis flow without a mouse, inspect controls with a screen reader, and expose selected plot measurements as text/table data. Upstream toolkit support alone does not verify HumTrace's custom UI.

The licensing tradeoff remains open. JUCE's AGPLv3/commercial path, Qt's LGPLv3/GPLv3/commercial module mix, wxWidgets' LGPL based exception, and SDL2 plus Dear ImGui's zlib/MIT terms have different implications. The product's chosen license, distribution model, module selection, contributor/developer context, and any third party test tooling must be known before finalizing a toolkit. This evaluation does not select a license or toolkit.

## Sources

Official project repositories or project documentation, accessed 2026-10-08.

1. JUCE, [Features](https://juce.com/features/).
2. JUCE, [CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md).
3. JUCE, [`juce::UnitTest`](https://docs.juce.com/master/classjuce_1_1UnitTest.html) and [`juce::UnitTestRunner`](https://docs.juce.com/master/classjuce_1_1UnitTestRunner.html).
4. JUCE, [LICENSE.md](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md).
5. JUCE, [JUCE 9 EULA](https://juce.com/legal/juce-9-licence/).
6. Qt, [Qt 6.12 Supported Platforms](https://doc.qt.io/qt-6/supported-platforms.html).
7. Qt, [Build with CMake](https://doc.qt.io/qt-6/cmake-manual.html).
8. Qt, [Accessibility](https://doc.qt.io/qt-6/accessible.html).
9. Qt, [`QAccessible`](https://doc.qt.io/qt-6/qaccessible.html).
10. Qt, [Qt Test module](https://doc.qt.io/qt-6/qttest-index.html).
11. Qt, [Qt Test overview: CMake and CTest](https://doc.qt.io/qt-6.12/qtest-overview.html).
12. Qt, [Qt Licensing](https://doc.qt.io/qt-6/licensing.html).
13. wxWidgets, [Overview](https://wxwidgets.org/about/).
14. wxWidgets, [CMake sample](https://github.com/wxWidgets/wxWidgets/blob/master/samples/minimal/CMakeLists.txt).
15. wxWidgets, [`wxAccessible` in current window API](https://github.com/wxWidgets/wxWidgets/blob/master/include/wx/window.h).
16. wxWidgets, [Accessibility tutorial](https://wxwidgets.org/docs/tutorials/accessibility/).
17. wxWidgets, [Development and automated testing](https://wxwidgets.org/develop/).
18. wxWidgets, [Licence](https://wxwidgets.org/about/licence/).
19. wxWidgets, [Supported platforms and classes](https://wxwidgets.org/docs/).
20. SDL, [SDL2 platforms](https://wiki.libsdl.org/SDL2/README-platforms) and [SDL2 licensing FAQ](https://wiki.libsdl.org/SDL2/FAQLicensing).
21. Dear ImGui, [Backend documentation](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md).
22. Dear ImGui, [Accessibility feature request](https://github.com/ocornut/imgui/issues/8022).
23. SDL, [SDL2 Installation and tests](https://wiki.libsdl.org/SDL2/Installation).
24. Dear ImGui, [MIT license](https://github.com/ocornut/imgui/blob/master/LICENSE.txt).
25. Dear ImGui, [Test Engine repository and license summary](https://github.com/ocornut/imgui_test_engine) and [Test Engine license](https://github.com/ocornut/imgui_test_engine/blob/main/imgui_test_engine/LICENSE.txt).
