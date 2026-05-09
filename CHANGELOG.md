# Changelog
## vertex-note-v0.2.0 (2026-05-08)

### ✨ Features
* Advance vertexnote geometry editing ([78def1a](https://github.com/saitatter/vertex-note/commit/78def1a8c551bf9d6f6af2182c2633aee6ec6262))

### 🐛 Fixes
* Harden geometry metadata parsing ([91f96eb](https://github.com/saitatter/vertex-note/commit/91f96ebff0e68ed18a199e8e9b56b1f19b0393e6))

### 🧰 CI & Build
* Align release workflow assets ([4123416](https://github.com/saitatter/vertex-note/commit/4123416a38e8176063351248cb3e20bdb9152694))
* Publish windows installer release asset ([fd61e49](https://github.com/saitatter/vertex-note/commit/fd61e49415371d25f6190432d7dd87f9e71e903a))
* Add windows portable release asset ([8494067](https://github.com/saitatter/vertex-note/commit/8494067bd5ed2d2296a2b20078fd5ea158da8a49))
* Make release workflow manual ([dc9b908](https://github.com/saitatter/vertex-note/commit/dc9b908d287723199ae6fea47ec2ff82358d3de8))
* Add dependabot updates ([b9066e8](https://github.com/saitatter/vertex-note/commit/b9066e86474458172bc1d04a6de427b1dec59807))

### 📚 Docs
* Add semantic release notes template ([9efaa93](https://github.com/saitatter/vertex-note/commit/9efaa93d0df9018a7eec7c21fecd530a68847b5d))


## vertex-note-v0.1.0 (2026-05-07)

### ✨ Features
* Add geometry constraint model ([3363fad](https://github.com/saitatter/vertex-note/commit/3363fad4b5c52f1c56446e4bce68a57d1fc5dded))
* Add vertex rectangle tool ([b2dc407](https://github.com/saitatter/vertex-note/commit/b2dc407b9a2f3e078a38540afaa619dad1fadb3a))
* Assign ids to geometry objects ([ba5cf27](https://github.com/saitatter/vertex-note/commit/ba5cf279c73484d8d698f04761b7fabe5ea600c2))
* Add click-based vertex polyline tool ([9ab0ad0](https://github.com/saitatter/vertex-note/commit/9ab0ad013af22ec5097064b37681458fca2895d0))
* Snap to geometry intersections ([781de45](https://github.com/saitatter/vertex-note/commit/781de45e8d3411e6b0a6b77ff68030550319db58))
* Snap vertex line tool to geometry ([d70b306](https://github.com/saitatter/vertex-note/commit/d70b306cebb6bfb14ff5c3dc164601b33d553318))
* Add provider-based snap engine ([f4fc93b](https://github.com/saitatter/vertex-note/commit/f4fc93b12063b8e23dae270daeda533844df0c5c))
* Add click-based vertex line tool ([49b2393](https://github.com/saitatter/vertex-note/commit/49b2393f1a8be8b7558a2979c90d4c1d9d625545))
* Add geometry element model ([2c04a9c](https://github.com/saitatter/vertex-note/commit/2c04a9cfd750c79cfb4c286bd449da467a55b691))
* Initialize vertexnote geometry foundation ([e7bec85](https://github.com/saitatter/vertex-note/commit/e7bec856ebb6803c9b96d346dbce2dc531f24154))
* Improve windows installer directory handling ([00f0c4c](https://github.com/saitatter/vertex-note/commit/00f0c4c87c7c5c09d7568c6e253aeff62aef175e))
* Check whether the directory is empty before installtion ([00f0c4c](https://github.com/saitatter/vertex-note/commit/00f0c4c87c7c5c09d7568c6e253aeff62aef175e))
* Make page shadow rendering configurable via settings ([99ebf23](https://github.com/saitatter/vertex-note/commit/99ebf23c2a74db1727fb63fad66658b9eb46eea6))
* **graph:** Support bold lines every nth interval ([6d7b49d](https://github.com/saitatter/vertex-note/commit/6d7b49d0d3db7f0f67b69c17ada4f20099dca2c0))
  - Added new config options: `bli` (bold line interval), `blw` (bold line width)
- Implemented logic to render every Nth grid line with increased thickness
- Updated page template
* Add configurable text placeholder tool with lua api and ui ([a778f2b](https://github.com/saitatter/vertex-note/commit/a778f2bc1d183399dfa76c1e7aa3eb8cc5fcbb08))
  - Implement text placeholder tool with config file and toolbar integration
- Add Lua API for live updates of placeholder values
- Ensure robust handling of multi-line and long values (escaping, truncation, ellipsis)
* Add recolor/inversion of drawing ares ([fa80fa5](https://github.com/saitatter/vertex-note/commit/fa80fa542f02f2d094432db8ab0e7f4e7eaa4fbd))
  This allows the user to not only have the UI in a dark mode (like it's already possible) but also to get some sort of dark mode for the drawing area. We achieve this by inverting (+ some recoloring) the drawing area (and optionally the previews shown in the sidebar as well).
  Recoloring works in three steps: 1. Invert all colors 2. Scale the color spectrum to fit into the range of light and dark
   color (these are the two parameters of the recoloring) 3. Move the color spectrum to really be in that range.
  This results in white areas being mapped to the *dark* color and black areas being mapped to the *light* color. Everything in between is interpolated to fit the color range.
  All operations are applied at the RGB color channels individually (no brightness/hue/saturartion calculation).
  For the parametrization this adds four new settings (to the settings dialog and the config file): 1. enable recoloring of the drawing area 2. enable recoloring of the previews in the sidebar 3. light color 4. dark color
  Also if recoloring is enabled, we add an indicator on top of the color buttons in the toolbar, of how the colors of the current palette will be visible on the inverted drawing area.
* Enable setting the color palette in the settings dialog ([56e9a08](https://github.com/saitatter/vertex-note/commit/56e9a08df4d0081a8bfb1549bf385770d0b91fb7))
* Enable changing palette colors dynamically ([39ef982](https://github.com/saitatter/vertex-note/commit/39ef98261355f2adcffd511f0ae17659781b8758))
  - palettes can be changed by changing `colorPaletteSetting`
- the changes are also reflected when customizing the toolbar
- two palettes are provided together with the application
  - VertexNote Palette
  - Xournal Pallete (currently the default)
* Add various options to create empty last page ([2f2e7c6](https://github.com/saitatter/vertex-note/commit/2f2e7c66139438a71d1bb375ebaf4484815a835e))
* Add duplicate page option under journal menu ([5aca1a7](https://github.com/saitatter/vertex-note/commit/5aca1a7b738f275aa26227b40966b212d1d001c2))
* Min size for shape recognizer now customizable ([3613fb6](https://github.com/saitatter/vertex-note/commit/3613fb6b7703a323c45bb56e4c799a88ce495a01))
  For some users, using the shape recognizer was problematic because it activated when writing letters. Now, users can set a minimum size: the shape recognition is disabled for small enough strokes.
* Default latex text now configurable ([aef6205](https://github.com/saitatter/vertex-note/commit/aef62050352864716b81327a2ffc1b0d0f6590d4))

### 🐛 Fixes
* Address geometry snapping review feedback ([1ad633b](https://github.com/saitatter/vertex-note/commit/1ad633bd38225809bfeb78eb1da8a22479691936))
* Adjustments for msvc stl ([f78d71d](https://github.com/saitatter/vertex-note/commit/f78d71df5a1391ef5e5ceff405ebe8cb85a5e46e))
* **plugin:** Add backend parameter to app.export() for pdf export ([6e36afd](https://github.com/saitatter/vertex-note/commit/6e36afdf5e40461b86acdbbbce953b8acbc81f4b))
  Exposes the export backend (qpdf/cairo) as a parameter to the Lua app.export() API. The Cairo backend correctly handles cropped PDF pages where the crop box origin differs from the media box, fixing text shifting on export.
  Fixes #7316
* Make sure the instdir end with xournal++ ([00f0c4c](https://github.com/saitatter/vertex-note/commit/00f0c4c87c7c5c09d7568c6e253aeff62aef175e))
* Remove tool_cap_ruler from pdf text selection tools ([0c4e598](https://github.com/saitatter/vertex-note/commit/0c4e598f89974647681d0d5919a71ee9cb7660c3))
  - also fixes saveSettings to only persist drawingType for tools with shape capabilities, preventing stale entries
* Plugin toolbar buttons disappearing in overflow menu ([4981dcd](https://github.com/saitatter/vertex-note/commit/4981dcde1ec1027d6ec3f295277463c192b12837))
* **icons:** Add missing xopp-draw-double-arrow icon to lucide theme ([7d23e1b](https://github.com/saitatter/vertex-note/commit/7d23e1bcfe831597863aaeb05fc6539ff9f00f5f))
* Argument forceopen in control::openfile has no effect ([32d2a47](https://github.com/saitatter/vertex-note/commit/32d2a4790a26222ef4ed5f63c4476108246ffed2))
* Fix light color as recolor.light ([146f7bd](https://github.com/saitatter/vertex-note/commit/146f7bdeb95f11ad6fc45f66feeaadbd67cc9d65))
* **luapi:** Preserve tool alpha when changing color via lua api #6883 ([76e3668](https://github.com/saitatter/vertex-note/commit/76e366814281fac100d695cd18e1e6db96274b46))
* **windows:** Correctly handle utf-8 command-line arguments in vertexnote-wrapper ([050b2a5](https://github.com/saitatter/vertex-note/commit/050b2a551609d6e03a90b1da4a54a2abed4d742b))
* Prevent invalid bounds in floatingtoolbox clamping ([8ccef78](https://github.com/saitatter/vertex-note/commit/8ccef78a3873c684499ca4232ed2e698749f629e))
* Use util::getdatapath() instead of hardcoded path for gladesearchpath ([67289c8](https://github.com/saitatter/vertex-note/commit/67289c8502a554a72e67b932e8b311bfc0c2232b))
* Removing passing stroketool as int to `g_warning` ([e340499](https://github.com/saitatter/vertex-note/commit/e3404992e3561e4aa4f8d9c711484bbd595cf104))
* Apply suggestions ([ca5c595](https://github.com/saitatter/vertex-note/commit/ca5c595ac656aedfcfd519c3707ce4b667dc9a27))
  Co-authored-by: Roland Lötscher <40485433+rolandlo@users.noreply.github.com>
* Color diagnostics now properly show with ninja ([a575bfa](https://github.com/saitatter/vertex-note/commit/a575bfa63acea36b7eb76f306be18e5f6b770f99))
* Declare a default virtual destructor in xmlnode ([6e7d54c](https://github.com/saitatter/vertex-note/commit/6e7d54ceeb79d787dece6d7f83583a47641ffc28))
* Properly validate version ([bd5cfe2](https://github.com/saitatter/vertex-note/commit/bd5cfe2e7e3b07a8c87b2989dc51c1189c06fd19))
  - `validate_version` properly validates against passed version value
- `validate_version` ensures an error is shown if no version is passed
- This also improves formatting of help menu
* Run release script through shellcheck ([f4e5d6b](https://github.com/saitatter/vertex-note/commit/f4e5d6b37ff81a6e28579bf45c2cc92646bc4f92))
* Prevent segfault when calling `g_object_get` ([c6bd41e](https://github.com/saitatter/vertex-note/commit/c6bd41e31d94af77c51e069842ceb03066b4ce4a))
* Bind ctrl+shift+z to redo ([6303fac](https://github.com/saitatter/vertex-note/commit/6303fac0fdb26aaadecb041546bb55412da54092))
* Ensure zsync artifact is published ([1531a44](https://github.com/saitatter/vertex-note/commit/1531a444a7b7c205f6d3e867abf420f98c1dc276))
* Crash on startup with no recent files ([57654c0](https://github.com/saitatter/vertex-note/commit/57654c0105ba058699c7869d485f095940d17e3e))
  - Add nullptr check to getMostRecent method
- Add nullptr check to on_startup function
- Change method descriptions to make explicit that nullptrs can be returned
  Fixes #3734

### ♻️ Refactors
* Remove global variables and use controller for placeholder tool and lua api ([7958070](https://github.com/saitatter/vertex-note/commit/795807043ae7ce383d88d0bbeef1b0d2e5cb9ca3))
  - Eliminate global g_control and g_textPlaceholderConfig usage
- Access TextPlaceholderConfig via Control instance throughout codebase
- Update Lua API to use Plugin context for controller access
- Improve code structure to follow MVC and best practices
- Clean up includes and function signatures for maintainability
* **Util:** Add dpi_normalization_factor ([de118fe](https://github.com/saitatter/vertex-note/commit/de118fed1ac29bdb92f110b46e153c95f84483ca))
  This spares us from hardcoding it everywhere.
  fixes #1368

### 🧰 CI & Build
* Properly obtain origin url from git ([0a7714e](https://github.com/saitatter/vertex-note/commit/0a7714e9263a5d43fbd00d847871f1732eca5722))
* Use findbacktrace.cmake instead of just assuming backtrace is available ([d65ebb3](https://github.com/saitatter/vertex-note/commit/d65ebb306d244f8bd6b6f5042924f1a69de3859e))
  fixes #1462
* Remove unused package ([00f0c4c](https://github.com/saitatter/vertex-note/commit/00f0c4c87c7c5c09d7568c6e253aeff62aef175e))
* Improve const-correctness ([194399c](https://github.com/saitatter/vertex-note/commit/194399c42d80d660eaac8427a937e50425102b2d))
* Remove redundancies in `.gitignore` ([3971c1a](https://github.com/saitatter/vertex-note/commit/3971c1a051293f6ef45f68a80ba49b96354062dd))
* Replace sprintf with placeholderstring in `aboutdialog.cpp` ([2222e53](https://github.com/saitatter/vertex-note/commit/2222e53421bc467dc3a217f02f546f129ce136fb))
* Use `std::numeric_limits` over macros ([907a0bd](https://github.com/saitatter/vertex-note/commit/907a0bd5c835b06f638d818172602d5087610490))
* Consistenize name of except var or remove if not in use ([d77a660](https://github.com/saitatter/vertex-note/commit/d77a660660269770600c13f9ffa44dcee4538832))
* Catch exceptions by const lvalue reference ([716a7e3](https://github.com/saitatter/vertex-note/commit/716a7e30504b64ae1151041dd0cd90eeff2f4dbb))
* Clang-format ([3ac6d4d](https://github.com/saitatter/vertex-note/commit/3ac6d4d770a5193e9e73a7ff5dd50961b0a2bb1c))
* Use default token for semantic release ([ab56508](https://github.com/saitatter/vertex-note/commit/ab5650815c8284c87bfb710fb642619740444444))
* Portable windows installation in release build ([ec18312](https://github.com/saitatter/vertex-note/commit/ec183123d4692d9a4d07afd8d9ecc2cb07926622))
* Update windows vm image to windows-2019 ([9ccfea3](https://github.com/saitatter/vertex-note/commit/9ccfea39075af85afe8b5deed37d96029525328d))
  Switch away from deprecated vs2017-win2016 image on 1.1.x branch. We already switched to windows-2019 on master branch.
* Remove cppunit download from mac build ([0b9324c](https://github.com/saitatter/vertex-note/commit/0b9324c8c5356645b48ed137429a24e4ca3c4283))
  cppunit is no longer used for testing. Removing it from the build step reduces Mac build times by about 2 minutes.
* Update windows agents ([8e39fcb](https://github.com/saitatter/vertex-note/commit/8e39fcb0ab7af14af782128df1908f40725b39b5))
  vs2017-win2016 is being removed, so switch to windows-2019 in all Windows builds.
* Fix full release trigger conditions ([eb34a02](https://github.com/saitatter/vertex-note/commit/eb34a02f643588e2c4d721d0287b133b432c34a8))
* Fix windows releases (attempt) ([0d2c33b](https://github.com/saitatter/vertex-note/commit/0d2c33be1fc1eca8eb7f9ecede2707123c1ce84b))
  The issue seems to be that libcrypto and libssl are already included in the Windows CI image by default, causing them to not be copied. This patch ensures that they're copied if the application is linked with the system libcrypto/ssl.
* Export project version to a generated file ([252e688](https://github.com/saitatter/vertex-note/commit/252e688fa0a75a862dac64860264a61627a8fc7d))
* Only apply clang-format to changed lines ([10b2deb](https://github.com/saitatter/vertex-note/commit/10b2debfe9b17fdd19c090eee57dc4252df6c574))

### 📚 Docs
* Add windows mingw64 local development setup ([786b31a](https://github.com/saitatter/vertex-note/commit/786b31a325f81c2d518816957d52c3d3a5943dc7))
* Update build requirements for c++20 and ninja ([7cc4be9](https://github.com/saitatter/vertex-note/commit/7cc4be9d2ad9c60f1628ffd073a6fc72548a5692))
* **lua:** Document registerplaceholder and setplaceholdervalue for toolbar labels ([b28046c](https://github.com/saitatter/vertex-note/commit/b28046caf3127e49c1701c836384eb639daa70b4))
* Fix whitespace, heading level issues ([81ca4db](https://github.com/saitatter/vertex-note/commit/81ca4db4aabbb19c5c3ecd771f9d9185d20f3bb9))
* Apply feedback ([d1806f8](https://github.com/saitatter/vertex-note/commit/d1806f8a8b7de0c015de9461c175f48828959ca3))
* Organize docs ([78e9b0c](https://github.com/saitatter/vertex-note/commit/78e9b0c03dd165f131c622b012d2d4df82386084))
* Correct minimum cmake version ([449cb25](https://github.com/saitatter/vertex-note/commit/449cb255b2b0ed58b51c2dcac2bce6c7804bb099))

### 🧪 Tests
* Add gtk based ui tests for the settingsdialog ([046105a](https://github.com/saitatter/vertex-note/commit/046105ad00f9cb387963b12704c38b6ba1f2080b))
