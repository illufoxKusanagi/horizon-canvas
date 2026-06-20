# Horizon Canvas

<p align="center">
  <strong>Horizon Canvas</strong> is a fast, OpenCL-accelerated vector painting and memory injection utility built specifically for <strong>Forza Horizon 6</strong>.
</p>

---

## 1. What is Horizon Canvas?

In Forza Horizon 6, the Vinyl Group Editor allows players to construct custom liveries, logos, and decals. However, recreating complex graphics, anime designs, or photorealistic badges manually is slow and difficult.

Horizon Canvas automates this process:

1.  **Generate:** It takes any standard digital image (`.png`, `.jpg`, `.jpeg`, `.bmp`) and translates it into overlapping geometric primitives (semi-transparent rectangles and ellipses).
2.  **Optimize:** Using a highly optimized GPU-bound scoring loop, it evaluates millions of random shape placements to reduce visual differences down to near-zero.
3.  **Inject:** Instead of slow keystroke macros (which can desynchronize and take hours), it connects directly to the game process, locates the editor structures, and writes the shape coordinates directly into the livery layers in a matter of seconds.

---

## 2. Installation & Quick Start

Horizon Canvas is built and compiled into a single standalone executable using Enigma Virtual Box.
*   **No Installation Required:** You do **not** need to install python dependencies, compile libraries, or configure local paths.
*   **How to Run:**
    1.  Go to the **Releases** page of this repository and download the latest `HorizonCanvas.exe`.
    2.  Place the `.exe` inside any writable folder on your computer (e.g. `Desktop\Horizon Canvas`).
    3.  Double-click `HorizonCanvas.exe` to launch the application.
    4.  *Note:* For memory injection, you may need to right-click the `.exe` and select **Run as Administrator** to allow process-level RAM access.

---

## 3. Key Features & Controls

| Feature                     | Description                                                                    | End-User Benefit                                                       |
| :-------------------------- | :----------------------------------------------------------------------------- | :--------------------------------------------------------------------- |
| **GPU/OpenCL Accelerator**  | Parallel evaluation of up to 500,000 shapes per second using graphics drivers. | Speeds up drawing optimization from hours to seconds.                  |
| **Sticker Mode Detection**  | Automatically detects transparent PNG margins.                                 | Restricts shapes to drawing inside your logo's boundaries only.        |
| **Safe Canvas Margins**     | Applies an invisible 8% boundary padding.                                      | Prevents texture stretching/smearing bugs at the edges of Forza cars.  |
| **Seamless Session Resume** | Checkpoints are saved continuously to JSON files.                              | Pick up exactly where you left off, even after restarts.               |
| **Direct Memory Writer**    | Attach to the active process memory using standard Windows APIs.               | Imports thousands of vector layers into the game canvas instantly.     |
| **Registry Presets**        | Save and load manual presets with override protection.                         | Saves your customized values safely without losing your configuration. |

---

## 4. How to Prepare Your Template in Forza Horizon 6 (Crucial)

Before you can write vector graphics into the game's active RAM, you must prepare a "template" canvas containing enough shapes for Horizon Canvas to overwrite.

### How to Make a Template:

1.  Launch **Forza Horizon 6** and go to **Garage** ➔ **Designs & Paints** ➔ **Create Vinyl Group**.
2.  Add a single simple shape (like a default **Sphere**).
3.  Duplicate that shape. Copy/paste multiple times to create a massive block of spheres (e.g. 500, 1000, 1500, or up to 3000 shapes).
    - _Tip:_ Create 100 shapes, select all, copy, and insert. You can populate a 3,000-layer canvas in less than a minute.
4.  **Save** this vinyl design with a recognizable name, like `Canvas 3000 Spheres`.

### Ungrouping is Mandatory:

- Before you start the memory write in Horizon Canvas, you must open your template, select all shapes, and click **Ungroup**. If the shapes remain grouped, the game memory shifts, and the injector will fail.

---

## 5. Preprocessing Modes Guide

Before drawing shapes, select a preprocessing filter to clean or prepare your target image:

| Mode        | Visual Action                                                | Best Used For                                                                 |
| :---------- | :----------------------------------------------------------- | :---------------------------------------------------------------------------- |
| **None**    | Passes the target image directly to the generator.           | Default logos, standard illustrations, crisp illustrations.                   |
| **Blur**    | Applies a fast 5x5 box-blur to smooth out color gradients.   | Photos or complex designs. Reduces visual noise so fewer shapes look cleaner. |
| **Outline** | Erodes transparency boundaries 2px inwards to isolate edges. | Outlines, borders, or sticker decals where only outlines are painted.         |

---

## 6. Step-by-Step Instructions

### Step 1: Generate the Shapes

1.  Navigate to the **Generate** page on the sidebar navigation menu.
2.  Drag and drop your image directly onto the drop widget, or click **Choose Image...** to browse.
3.  Select a **Quality Preset** from the dropdown menu:
    - _Fastest / Fast:_ Quick composition drafts.
    - _Medium / Best:_ Recommended defaults for daily designs.
    - _Extreme / Rip my GPU:_ Heavy GPGPU calculations for maximum detail (requires dedicated graphics card).
4.  _(Optional)_ Enable **"Use custom settings"** to adjust layer count limits, random samples, blur modes, or checkpoint frequencies.
5.  Click **Start Generation**. Progress will save continuously as a `_shapes.json` file in the folder of your source image.

### Step 2: Custom Presets & Loading

- If you have checked **"Use custom settings"**, you can save your setup as a new preset by clicking **Save as preset**.
- **The Safe Guard:** When "Use custom settings" is checked, changing the dropdown combobox will **not** overwrite your inputs. If you want to load a preset's numbers over your custom edits, select it in the combobox and click the **Load Preset** button.

### Step 3: Resuming Your Project

1.  Go to the **Resume** page.
2.  Drag and drop your `_shapes.json` checkpoint file.
3.  The page will display a preview of the shapes, the count, and automatically default your quality profile to **Extreme** to maximize continuity.
4.  Click **Start Generation** to continue drawing.

### Step 4: Injecting Into Forza Horizon 6

1.  Launch **Forza Horizon 6** and enter the **Create Vinyl Group** editor canvas.
2.  Load your prepared template group and click **Ungroup**. Keep the game active on this screen.
3.  In Horizon Canvas, navigate to the **Inject** page.
4.  Drag and drop your completed `_shapes.json` file.
5.  Verify the details on the visual preview.
6.  Click **Inject to FH6**. Wait for the memory scanner to scan the process offset and write the shapes. Once the status bar turns green, the injection is complete!

---

## 7. Detailed Troubleshooting FAQ

| Problem                                    | Potential Cause                                         | How to Fix                                                                                                                       |
| :----------------------------------------- | :------------------------------------------------------ | :------------------------------------------------------------------------------------------------------------------------------- |
| **Build/Launch Failures**                  | Missing standard runtimes.                              | Install [Microsoft Visual C++ 2015 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) (both x86 and x64 versions). |
| **"No confident match" or scanning hangs** | Vinyl editor isn't open or template is still grouped.   | Ensure you are inside the "Create Vinyl Group" screen and that you have clicked **Ungroup** on the shapes.                       |
| **OpenCL GPU generation error**            | Outdated graphics drivers or using integrated graphics. | Update your Nvidia/AMD/Intel graphics card drivers. Switch the backend to CPU mode if your GPU doesn't support OpenCL.           |
| **Result is blurry or pixelated**          | Low shape counts or low sample settings.                | Increase **Output layers** (up to 3000) and raise **Random samples** in custom settings.                                         |
| **Game client crashes during inject**      | Switched menus or drove car during injection.           | Keep Forza active on the canvas editor. Do not click anything inside the game or in the app during the memory write.             |

---

## 8. Dos and Don'ts

### ✅ DO

- **Do crop out blank space:** Crop margins from images before import. The optimizer wastes time trying to paint background zones.
- **Do let the engine run:** The longer the optimizer runs, the closer the RMS error drops to `0.0`, resulting in a cleaner image.
- **Do keep save JSONs safe:** Keep files in their original locations to avoid pathing errors during resume or injection.

### ❌ DON'T

- **Don't exceed 3,000 layers:** Forza Horizon 6 limits vinyl designs to 3,000 layers. The app restricts custom settings to prevent game engine rejections.
- **Don't run heavy profiles on integrated graphics:** High-stress OpenCL workloads on standard integrated laptop processors can crash display drivers.
- **Don't manually modify shape JSON files:** Editing shape vectors directly can corrupt the coordinate arrays and break the parser.

---

## 9. Safety & Ban Disclaimer

**Use this software entirely at your own risk.**

Horizon Canvas modifies the memory of a running Forza Horizon process to write coordinate values in the active livery canvas. It does not patch files, install custom drivers, modify permanent save game files, or bypass DRM.

However, **any live process memory modification is a violation of the Microsoft Services Agreement, Xbox Live Community Standards, and Playground Games' Terms of Use**. Sharing highly detailed generated vinyls publicly can lead to reports from other players. The developers and contributors accept **no liability or responsibility whatsoever** for account suspensions, temporary bans, or permanent game bans resulting from the use of this utility.

---

## 10. Credits & Acknowledgements

Horizon Canvas is built upon the discoveries, research, and open-source contributions of the Forza painting and procedural generation developer community:

- **[forza-painter](https://github.com/the-adawg/forza-painter)** by **`the_adawg`** — The original pioneering project that cracked process memory identification and vector overrides for Forza Horizon 4 and 5.
- **[forza-painter-fh6](https://github.com/bvzrays/forza-painter-fh6)** by **`bvzrays`** — Standardized offset lookups, float sizing values, and template locators for Forza Horizon 6.
- **[forza-painter-geometrize-gpu](https://github.com/zjl88858/forza-painter-geometrize-gpu)** by **`zjl88858`** — Contributed upstream references for parallel GPU/OpenCL candidate evaluation kernels.
- **[ForzaDesigner6 (FD6)](https://github.com/tokyubevoxelverse/ForzaDesigner6)** by **`tokyubevoxelverse`** — Explored RTTI scanner definitions (`.?AVCLiveryGroup@@`) for scanning offset fallbacks and file-based exports.
