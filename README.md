# 🌬️ Breezy — Game Prototype with Breath Mechanics

> An interactive, breath-controlled web game prototype featuring real-time audio analysis, camera motion detection, and optional Arduino hardware sensor integration.

---

## 🌟 Highlights

- 🎤 **Breath-Driven Mechanics**: Uses browser microphone audio processing and camera analysis to measure breath intake and exhale length.
- 🌲 **Chamki Firefly Forest**: Guide your character through magical forest environments by controlling your breathing.
- 📊 **Breathing Analytics & Reports**: Live breathing statistics, rhythm evaluation, and session reports for players and grownups.
- 🎵 **Adaptive Sound Engine**: Dynamic ambient soundtrack layers and sound effects responsive to player actions.
- 🔌 **Hardware Support (Optional)**: Includes Arduino sketches (`arduino/`) for HX710B breath flute pressure sensors and serial simulators.
- ⚡ **No account required**: Open the game directly; profile and progress stay on the device.
- ⚡ **Zero-Config Vercel Ready**: Ready to deploy directly to Vercel or any static hosting platform.

---

## 🚀 Quick Start (Run Locally)

You can run Breezy using any local HTTP static file server.

### Option 1: Using Python (Simplest, no install needed)

```bash
# In the project root directory:
python3 -m http.server 8000
```
Open your browser and navigate to:
👉 **[http://localhost:8000](http://localhost:8000)** (or directly `http://localhost:8000/Breezy%20Home.dc.html`)

---

### Option 2: Using Node.js / npm

```bash
# Start local static server
npm start
```
Open your browser at **[http://localhost:8000](http://localhost:8000)**.

---

### Option 3: VS Code / IDE Live Server
1. Install the **Live Server** extension in VS Code.
2. Right-click `index.html` or `Breezy Home.dc.html` and click **"Open with Live Server"**.

---

## ☁️ Deploying to Vercel

This repository is pre-configured for Vercel deployment with `vercel.json`.

1. Push this repository to GitHub: `https://github.com/shelar1423/breezy.git`
2. Go to [Vercel Dashboard](https://vercel.com/new).
3. Click **"Import Project"** and select `shelar1423/breezy`.
4. Leave all build settings as default (Framework Preset: **Other**, Root Directory: `./`).
5. Click **"Deploy"** — your live game will be instantly available at `https://<your-project>.vercel.app`!

---

## 📁 Project Architecture & Structure

```
├── Breezy Home.dc.html            # Main game launcher, level select, profile & settings
├── Chamki Firefly Forest.dc.html  # Main gameplay scene with breath detection & firefly collection
├── Breathing Report.dc.html       # Player breathing report and session statistics
├── Chamki Start.dc.html           # Story intro and mission launch pad
├── Button Kit.dc.html             # Reusable UI component kit
├── cave-ribbon.dc.html            # Cave biome (ribbon route)
├── cave-rock.dc.html              # Cave biome (rock route)
├── cave-vines.dc.html             # Cave biome (vines route)
├── map-compare.dc.html            # Level map comparison tool
├── map-loop.dc.html               # Level looping stage map
├── map-path.dc.html               # Level path stage map
│
├── index.html                     # Root entrypoint redirecting to Breezy Home
├── support.js                     # Component renderer & reactive runtime
├── doc-page.js                    # Document page helpers
├── vercel.json                    # Vercel deployment & routing configuration
├── package.json                   # Local development server scripts
│
├── assets/                        # Game art, sprites, backgrounds & sounds
│   ├── audio/                     # BGM music loops, ambient layers, and SFX
│   ├── game-audio.js              # Web Audio API engine (music, layers, sfx)
│   ├── game-camera.js             # WebCam motion and face calibration
│   └── char/                      # Character avatars and sprites
│
├── arduino/                       # Physical breath sensor firmware
│   ├── breath_flute_hx710b/       # HX710B differential pressure sensor reader
│   └── breath_sim_uno/            # Arduino Uno breath simulator
│
├── uploads/                       # Storyboards, pitch docs, stage assets
└── shots/                         # Concept captures and reference images
```

---

## 🎮 How to Play

1. **Launch the game**: Visit `http://localhost:8000` and click **"PLAY"**.
2. **Microphone / Breath Calibration**:
   - Allow microphone permissions when prompted.
   - Gently blow / exhale into your microphone or hardware flute to power up your firefly light.
3. **Inhale & Exhale Cycles**:
   - Follow the rhythmic breathing prompts on screen.
   - Hold steady breaths to illuminate dark forest pathways and wake sleeping creatures.
4. **Track Progress**:
   - Check your profile at the top-left of the Home screen to view your LVL, XP, and detailed breathing consistency reports.

---

## 🛠️ Arduino Hardware Integration (Optional)

If using a custom physical breath controller (e.g. pressure sensor flute):
- Check [`arduino/breath_flute_hx710b/`](file:///Users/digvijay/Downloads/Game%20Prototype%20With%20Breath%20Mechanics/arduino/breath_flute_hx710b/) for the HX710B sensor sketch.
- Connect via Web Serial API or serial bridge to stream live breath pressure values directly into the game.

---

## 📄 License

This project is created for prototype and educational purposes. MIT License.
