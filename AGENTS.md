# AGENTS.md

## Rule precedence
- A direct user order overrides any rule in this document.
- The user is in full command and may cancel all these rules at any moment.

## Refactoring stance
- The default minimal-diff behavior does not apply here: the agent serves the global vision of the codebase, never a narrow correction perimeter, and refactors everything the change touches whenever structure, factorization, or generality requires it.

## Project Overview
Qt-based 3D engine with geo-referenced entities (C++/Qt), built with CMake for Qt6 (see `README.md`). Layout:
- `qt-plus/` (git submodule): shared Qt utility library (`CLogger`, `CXMLNode`, `CSingleton`, `CInterpolator`, geotools, ...). Engine equivalent of a commons module — check here first before adding helpers.
- `quick3d/` (engine library): `source/` (`animation`, `base`, `components`, `input`, `math`, `mesh`, `render`, `terrain`, `utils`, `zip`), shaders in `Quick3DShaders.qrc`, docs in `quick3d/doc/`.
- `quick3dtest/` (test/demo application): GUI (`Quick3DTest.ui`), `main.cpp` (`--scene`, `run-tests`), scenes/models/textures in `quick3dtest/bin/`.
- `third/`: vendored third-party code (`SFML-1.6`, `unzip11`, `zlib` as git submodule). Do not add new third-party dependencies without asking.
- `quick3d/blender/`: Blender Q3D export script (Python, existing exception to the C++ rule below).

Fresh clones have empty `qt-plus/` and `third/zlib/` until submodules are initialized (`git submodule update --init --recursive`). Rendering is `QOpenGLWidget`-based, the engine targets Qt6 (Qt 6.2.4 on this machine); the legacy `.pro`/`.pri` files are kept for QtCreator users but the reference Linux build is CMake. `QGL*` classes are gone: use `QOpenGL*`.

## Release vs debug
- Work and build in RELEASE mode. DEBUG is used when tracking bugs (hence its name) : either when debug logs or an attachement with a debugger is needed.

## Communication Guidelines
- NEVER use emojis in responses.
- DON'T act like a human being with emotions, just be a machine.
- DON'T says "Great!", "Perfect!", "You're right" all the time.
- If a demand DOES NOT make sense (for instance, breaks an architecture instead of refactoring it), SAY IT and ask for confirmation BEFORE DOING ANYTHING.
- NEVER create a commit unless the user explicitly asks for it in the current conversation.

## Architecture and Reuse Rules
- Bidirectional coupling is STRICTLY FORBIDDEN, both when writing code from scratch and when delivering a fix. Keep dependencies unidirectional and break cycles instead of introducing or preserving them.
- Any behavior likely to appear in multiple places (filtering, interpolation, averaging, logging, XML access, geo math) MUST be implemented as a reusable module in `qt-plus/source/cpp` or, if engine-specific, in `quick3d/source/utils`.
- Before adding local logic in a component/generator/subsystem, check `qt-plus/source/cpp` and `quick3d/source/utils` first. If no suitable module exists, create a generic one and use it from the caller.
- Component code should only express policy/usage, not duplicate generic mechanics.
- For log-flood control, add a shared helper in `qt-plus`/`utils` and use it everywhere; do not hardcode ad-hoc counters/cooldowns inside modules.
- Container usage: this rule applies to the whole engine. Use Qt containers (`QVector`, `QMap`) and shared ownership (`QSP`, see below). Do not introduce custom linked-list infrastructure.
- Mutex ownership: this rule applies to the whole engine (worker queue in `CWorkerManager`, chunk mutex in `CWorldChunk`, ...). Never lock another object's mutex directly to inspect or mutate its internal state. Expose owner-side getters/setters/snapshot helpers, keep critical sections short, and never recurse or call callbacks/messages while holding structural object locks.
- Public headers: `quick3d_global.h` and the headers consumed by downstream apps (`Quick3DTest`). Keep them app-safe: no internals that break downstream includes, no platform-only constructs leaking into the shared include surface.

## Coding Conventions
- Types: Use the engine math types (`CGeoloc`, `CVector2/3/4`, `CMatrix4`, ...) and Qt types where they exist instead of introducing parallel raw-C representations. `double` for geo math.
- Dependencies: This is a Qt user-space project. Qt and the STL are allowed. Do NOT add new third-party libraries/modules (vendored or system) without asking; `third/` is the only place for vendored code.
- Debugging: Debug output is logged with `LOG_METHOD_DEBUG()` (qt-plus `CLogger`), warnings with `LOG_WARNING()`/`LOG_METHOD_WARNING()`, errors with `LOG_ERROR()`/`LOG_METHOD_ERROR()`.
- Test-app failure visibility: Any failure/error message shown on screen in `Quick3DTest` MUST also be emitted through `CLogger` with equivalent content. No failure must appear only on screen without a corresponding log entry.
- Logging: `CLogger` already prefixes method context via the `LOG_METHOD_*` macros. Keep messages short at `Warning` level and above.
- WARNING/ERROR semantics: `LOG_WARNING()` and `LOG_ERROR()` are human-facing alerts. They MUST stay short, actionable, and understandable without deep protocol knowledge.
- Diagnostic dumps: Detailed diagnostics (mesh internals, face/vertex dumps, retry traces, step-by-step instrumentation) MUST use `LOG_DEBUG()`/`LOG_METHOD_DEBUG()` only, never warning or error level.
- TEXT literals: Write log/UI messages directly as `QString`.
- Magic number: NEVER introduce magic numbers or magic strings. Always declare reusable values in named constants.
- Declaration order: Group declarations by type. 1: macros / 2: type definitions / 3: inline functions / 4: external functions / 5: other
- Function order: DO NOT OVERUSE forward declarations. Define functions before they are used.
- I18n: Write comments, console output and technical doc in english.
- Naming: PascalCase for classes/variables/members (`C`-prefixed classes, `m_`/`g_`/`p_` prefixes as in the existing code), SCREAMING_SNAKE_CASE for structs/defines.
- Naming clarity: In addition to using full words, every name must express its intent clearly and without ambiguity.
- Comments: For single-line comments, use `//`, not `/*`.
- Style: 4-space indentation. Match surrounding code instead of reformatting whole files.
- Pointer style: Never put a space between the type and the pointer asterisk. Write `char* Pointer`, never `char *Pointer`.
- Numbers: Hexadecimal for constant numbers, except for sizes, vectors, points and time.
- Number suffixes: Do not add numeric suffixes like `u` to constants; they are not wanted here.
- Documentation: Update `quick3d/doc/` when adding/modifying engine components. Both library `.pro` files run `qdoc` as a post-link step (`Quick3D.qdocconf`, `qt-plus.qdocconf`); keep the qdoc comments compiling.
- Documentation wording: Use timeless technical wording. Do not use temporal terms like "now", "currently", "at this time" in documentation/comments.
- Resource paths: Scenes, models and textures resolve relative to `quick3dtest/bin/` (`models/`, `textures/`, `*.xml`). Never hardcode absolute paths.
- Languages: C++ for engine and test app, avoid Python (the Blender Q3D export script is the existing exception).
- Unused parameters: Use `Q_UNUSED()` to suppress the "unused parameter" warning.
- Shared ownership: Components (`CComponent` inherits `QSharedData`) are held by `QSP` (`QExplicitlySharedDataPointer`, see `CQ3DConstants.h`). Null-check `QSP`s before use, cast with `QSP_CAST`, and NEVER wrap an already-owned `this` in a fresh `QSP` (second ownership chain = double-delete; see `TerrainSecondaryIssues.md` #1).
- Component lifetime: Never manually `delete` a component still referenced by `QSP`s. Clearing links goes through `clearLinks()`; terrain data through `clearTerrain()`.
- Threading: `CWorker`/`CWorkerManager` is the background-build pool. Never block a worker thread waiting on another worker (see `TerrainHolesAnalysis.md` #1). Plain-`bool` cross-thread flags are a known race (see `TerrainSecondaryIssues.md` #2): prefer atomics or mutexes for new code.
- Component factory: `CComponentFactory` instantiators return `nullptr` for unknown classes; callers MUST null-check before use.
- Clean code: No duplicate code. Create intermediate functions to avoid it. This also applies to data: create intermediate structures to avoid duplicating data.
- No globals: Before adding a global variable, ALWAYS ASK if permitted.
- Functions: Add a doxygen header to functions and separate all functions with a 75 character line such as : //
- File size: Keep source files under 1000 lines; split by responsibility before crossing this limit.
- Vocabulary:
  - NEVER use abbreviations; ALWAYS use full words (acronyms are OK).
  - No figurative language: respect the repository vocabulary, use every word in its primary (literal) sense. Name things for what they are, not for what they resemble.
  - No "directory" : use "folder".
  - Fixed-width types (`U8/I8`, `U16/I16`, `U32/I32`, `U64/I64`, `F32`, `F64`) in persistent data (Q3D format, height-field files), not register-sized `INT/UINT`.

### Header format
- Always respect the following format for classes :
```
class <export> CSomeClass : public CSomeParentClass
{
    friend class CSomeFriendClass;

public:

    //-------------------------------------------------------------------------------------------------
    // Constructors and destructor
    //-------------------------------------------------------------------------------------------------

    //!
    CSomeClass();

    //-------------------------------------------------------------------------------------------------
    // Setters
    //-------------------------------------------------------------------------------------------------

    //! Sets something
    void setSomeMember(QString sSomeString);

    //-------------------------------------------------------------------------------------------------
    // Getters
    //-------------------------------------------------------------------------------------------------

    //! Gets something
    QString getSomeMember();

    //-------------------------------------------------------------------------------------------------
    // Virtual methods
    //-------------------------------------------------------------------------------------------------

    virtual QString someVirtualMethod();

    //-------------------------------------------------------------------------------------------------
    // Overridden methods
    //-------------------------------------------------------------------------------------------------

    virtual QString someOverridenMethod() Q_DECL_OVERRIDE;

    //-------------------------------------------------------------------------------------------------
    // Control methods
    //-------------------------------------------------------------------------------------------------

    //! Does something
    void doSomething();

    //-------------------------------------------------------------------------------------------------
    // Properties
    //-------------------------------------------------------------------------------------------------

protected:

    QString   m_sSomeString1;
    QString   m_sSomeString2;
};
```

### Commit message
Describe the changes in a short sentence.

Test: append a second line with the test results: `Tests: <passed>/<total>`
Omit the line only when no test ran.

Sign: append a last line `Agent: <agent-name>`
(for example `Agent: opencode/muse-spark`) so authorship stays identifiable
in the log, whatever the commit prefix above.

Never leave a blank line in the commit message: lines follow each other directly.

## Code review
- When the user asks for a code review, review the target commit or commit range for OOP compliance, factorization, absence of duplicated code, and compliance with the Architecture and Reuse Rules plus the Coding Conventions of this document.
- Write the findings in english in a dedicated file under `quick3d/doc/` (for example `quick3d/doc/Review-<hash>.md`).
- Do not wrap issue lines at 80 characters: one issue uses one full line, however long the line is.
- Group issues by criticality under `Critical`, `Major`, and `Minor` headings.
- Give every issue a sequential number, a `file:line` location, and a checkbox (for example `- [ ] #12 \`quick3d/source/mesh/CWorldTerrain.cpp:1098\` — description`).

## Common Build Commands
- Dependencies: `./scripts/linux/deps.sh` (submodules + apt/dnf/pacman packages; needs `sudo`, which prompts for a password here)
- Build (shadow, like `galman`): `./scripts/linux/build.sh [--debug|--release]` (`cmake -S . -B build-<debug|release> -G Ninja`, then `cmake --build`). Builds `qt-plus`, then `Quick3D`, then `Quick3DTest`.
- Clean: `./scripts/linux/clean.sh [--debug|--release|--all]`
- Run the test app: `./scripts/linux/run.sh [--debug|--release] [-- <args>]` (logs to `tmp/run.log`), or directly `build-<debug|release>/quick3dtest/Quick3DTest[d] [--scene <file>]`, or `... run-tests` for the headless unit tests (`QT_QPA_PLATFORM=offscreen` works headless).

## Tool Execution Policy
- `tmp/` at the repo root is the folder for temporary/scratch files. It is gitignored; scripts must create it (`mkdir -p`) if missing.
- Never write scratch/generated artifacts into the repo without asking; keep the tree clean (`build-debug/`, `build-release/`, `tmp/` are gitignored).
- `sudo` prompts for a password on this machine: never assume elevation works; ask the user to run installs themselves or explicitly approve.
- Run builds sequentially, not in parallel.

## Debug output
- Engine logging goes through `CLogger` (qt-plus). `Quick3DTest run-tests` exercises components without opening the GUI.

## Documentation
- Build setup of the engine: `README.md`, `quick3d/doc/` (`Quick3D.qdocconf`, `Quick3DScene.xsd`), `qt-plus/README.md`

## Work Tracking
- Analysis and work notes, if any, live in `quick3d/doc/`.
- Keep these files up to date while working.

Core Components:
- qt-plus (`qt-plus/source/cpp`): Shared Qt utility library (logging, XML, singletons, interpolation, geo tools, web, QML tree). Git submodule.
- Quick3D (`quick3d/source/`): Engine library — `base` (components, workers, factory), `mesh` (world terrain/chunks, geometry), `terrain` (patches, height fields), `render` (`CGLWidgetScene`, scene, camera), `math`, `animation`, `components` (vehicles, controllers), `input`, `utils`, `zip`.
- Quick3DTest (`quick3dtest/`): GUI test application + QtTest suites (`quick3dtest/tests/`).
- Blender (`quick3d/blender/`): Q3D export script.
- third (`third/`): Vendored third-party code (`SFML-1.6`, `unzip11`, `zlib` submodule).

## Debug Workflow
1. Reproduce headless first when possible: `Quick3DTest run-tests`.
2. For terrain LOD/patch issues, read `quick3d/doc/TerrainHolesAnalysis.md` before re-investigating: the worker-scheduler starvation, the missing `paintRecurse()` fallback, and the premature child-pruning mechanisms are already documented there.
3. This machine has Qt 6.2 (system packages + `qmake6`); the reference build is CMake + Ninja via `scripts/linux/`.
4. To reproduce a user problem, drive the UI through the HTTP remote control server (`CRemoteControlServer`, port 8181 or `Q3D_REMOTE_PORT`): `/key?code=Z&action=press|release|click`, `/select`, `/view`, `/vehicle`. Never call quick3d internal APIs directly to simulate user input; the repro path must match what the user does.