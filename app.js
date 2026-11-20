// ============================================================================
// Crane Song Phoenix II - 1:1 Bit-Exact DSP & 7-Skin Interactive Studio Engine
// Plugin Creator / Developer: papielguapo
// ============================================================================

(function () {
    "use strict";

    // 1. Decode 24,576-float PHXTBLDATA extracted from PhoenixII.aaxplugin / PhoenixToo_Stereo.dll
    function decodePhxTable() {
        const b64 = window.PHOENIX_RAW_TABLE_B64 || "";
        const binStr = atob(b64);
        const bytes = new Uint8Array(binStr.length);
        for (let i = 0; i < binStr.length; i++) {
            bytes[i] = binStr.charCodeAt(i);
        }
        return new Float32Array(bytes.buffer);
    }

    const PHXTBLDATA = decodePhxTable();
    const SR_FACTORS_TABLE = [1.0, 0.5, 0.25];

    // Exact 5 Phoenix_Color_Types structures extracted from binary (0x20020 in PhoenixToo_Stereo.dll)
    const PHOENIX_COLOR_TYPES = [
        {
            name: "Luminescent",
            shortName: "Lumin",
            brightCoeffs: [
                [0.625, 0.1875],   // 1: Opal
                [0.4375, 0.3125],  // 2: Gold
                [0.1875, 0.375]    // 3: Sapphire
            ],
            driveScale: 0.25,
            preEmphasisGain: 0.75,
            satMixGain: 0.3125,
            dryCompGain: 0.0625,
            flagA: 1,
            tableOffset: 0
        },
        {
            name: "Iridescent",
            shortName: "Irid",
            brightCoeffs: [
                [0.625, 0.1875],   // 1: Opal
                [0.375, 0.3125],   // 2: Gold
                [0.3125, 0.5]      // 3: Sapphire
            ],
            driveScale: 0.25,
            preEmphasisGain: 0.875,
            satMixGain: 0.3125,
            dryCompGain: 0.0625,
            flagA: 1,
            tableOffset: 0
        },
        {
            name: "Radiant",
            shortName: "Radiant",
            brightCoeffs: [
                [0.75, 0.125],             // 1: Opal
                [0.4562990069389343, 0.375], // 2: Gold
                [0.375, 0.5]               // 3: Sapphire
            ],
            driveScale: 0.375,
            preEmphasisGain: 0.75,
            satMixGain: 0.1875,
            dryCompGain: 0.012500000186264515,
            flagA: 0,
            tableOffset: 8192
        },
        {
            name: "Luster",
            shortName: "Luster",
            brightCoeffs: [
                [0.75, 0.125],             // 1: Opal
                [0.4562990069389343, 0.375], // 2: Gold
                [0.375, 0.5625]            // 3: Sapphire
            ],
            driveScale: 1.0,
            preEmphasisGain: 0.6875,
            satMixGain: 0.2734389901161194,
            dryCompGain: 0.1171875,
            flagA: 0,
            tableOffset: 16384
        },
        {
            name: "Dark Essence",
            shortName: "DrkEs",
            brightCoeffs: [
                [0.75, 0.125],             // 1: Opal
                [0.4562990069389343, 0.375], // 2: Gold
                [0.375, 0.5625]            // 3: Sapphire
            ],
            driveScale: 0.375,
            preEmphasisGain: 0.75,
            satMixGain: 0.5625,
            dryCompGain: 0.012500000186264515,
            flagA: 0,
            tableOffset: 16384
        }
    ];

    const BRIGHTNESS_NAMES = ["Opal", "Gold", "Sapphire"];

    // 1:1 Table Lookup function disassembled from VA 0x1800108D0..0x1800109CE
    function lookupPhxTbl(val, tableOffset) {
        if (val > 0.999755859) val = 0.999755859;
        if (val < -1.0) val = -1.0;

        const fixed = (val * 2147483648.0) | 0;
        const intPart = fixed >> 19; // [-4096 .. +4095]
        let idx = intPart + tableOffset + 4096;
        if (idx < tableOffset) idx = tableOffset;
        if (idx > tableOffset + 8190) idx = tableOffset + 8190;

        const y0 = PHXTBLDATA[idx];
        const y1 = PHXTBLDATA[idx + 1];
        const frac = (fixed & 0x7FFFF) * 1.9073486328125e-6; // 1.0 / 524288.0
        return y0 + frac * (y1 - y0);
    }

    // Plugin State & A/B Slots
    const state = {
        skin: "cranesong",
        dualView: false,
        scale: 1.5,
        activeSlot: "A",
        params: {
            inputTrim: 0.0,   // -10.0 .. +10.0 dB
            process: 0.0,     // 0.0 .. 100.0 %
            outputTrim: 0.0,  // -6.0 .. +6.0 dB
            brightness: 2,    // 1=Opal, 2=Gold, 3=Sapphire
            type: 1,          // 1=Luminescent, 2=Iridescent, 3=Radiant, 4=Luster, 5=Dark Essence
            bypass: false
        },
        slotA: { inputTrim: 0.0, process: 0.0, outputTrim: 0.0, brightness: 2, type: 1, bypass: false },
        slotB: { inputTrim: 4.6, process: 100.0, outputTrim: 6.0, brightness: 3, type: 1, bypass: false },
        dspState: {
            x1L: 0.0, y1L: 0.0,
            x1R: 0.0, y1R: 0.0
        },
        meters: {
            inPeak: 0.0,
            outPeak: 0.0,
            driveAmt: 0.0
        }
    };

    const PRESETS = {
        default:      { inputTrim: 0.0,  process: 35.0, outputTrim: 0.0,  brightness: 2, type: 1 },
        vocal_warm:   { inputTrim: 1.2,  process: 48.0, outputTrim: -0.8, brightness: 1, type: 2 },
        drum_slam:    { inputTrim: 3.5,  process: 75.0, outputTrim: -2.2, brightness: 3, type: 3 },
        master_glue:  { inputTrim: 0.5,  process: 42.0, outputTrim: -0.4, brightness: 2, type: 4 },
        dark_vintage: { inputTrim: 2.0,  process: 68.0, outputTrim: -1.5, brightness: 1, type: 5 },
        bright_air:   { inputTrim: 0.8,  process: 52.0, outputTrim: -0.6, brightness: 3, type: 1 },
        hot_trafo:    { inputTrim: 5.5,  process: 88.0, outputTrim: -3.5, brightness: 2, type: 3 }
    };

    // 1:1 Stereo Audio Block Processor (VA 0x180010380..0x1800108CE)
    function processPhoenixBlock(inL, inR, outL, outR, numSamples, sampleRate, filterState) {
        const p = state.params;
        if (p.bypass) {
            let pk = 0;
            for (let i = 0; i < numSamples; i++) {
                outL[i] = inL[i];
                outR[i] = inR[i];
                const a = Math.max(Math.abs(inL[i]), Math.abs(inR[i]));
                if (a > pk) pk = a;
            }
            state.meters.inPeak = pk;
            state.meters.outPeak = pk;
            state.meters.driveAmt = 0;
            return;
        }

        const inTrimGain = Math.pow(10.0, p.inputTrim / 20.0);
        const processVal = Math.max(0.0, Math.min(100.0, p.process)) / 100.0;
        const outTrimGain = Math.pow(10.0, p.outputTrim / 20.0);

        let srIndex = 0;
        if (sampleRate >= 150000) srIndex = 2;
        else if (sampleRate >= 75000) srIndex = 1;
        const srFactor = SR_FACTORS_TABLE[srIndex];

        const tIdx = Math.max(1, Math.min(5, p.type));
        const bIdx = Math.max(1, Math.min(3, p.brightness));
        const isType4 = (tIdx === 4) ? 1 : 0;
        const type4Factor = 1.0 - isType4 * 0.5;

        const color = PHOENIX_COLOR_TYPES[tIdx - 1];
        const coeffPre = color.brightCoeffs[bIdx - 1][0];
        const coeffPost = color.brightCoeffs[bIdx - 1][1];

        const scaledProcess = processVal * color.driveScale;
        const effProcess = scaledProcess * type4Factor;

        let inPeak = 0.0;
        let outPeak = 0.0;
        let satEnergy = 0.0;

        const chansIn = [inL, inR];
        const chansOut = [outL, outR];
        const sxArr = [filterState.x1L, filterState.x1R];
        const syArr = [filterState.y1L, filterState.y1R];

        for (let ch = 0; ch < 2; ch++) {
            let sx1 = sxArr[ch];
            let sy1 = syArr[ch];
            const src = chansIn[ch];
            const dst = chansOut[ch];

            for (let i = 0; i < numSamples; i++) {
                const prevY1 = sy1;
                const rawIn = src[i];
                const inSample = rawIn * inTrimGain;
                if (Math.abs(inSample) > inPeak) inPeak = Math.abs(inSample);

                let x = inSample;
                // High-frequency pre-emphasis filter
                x = (x * (coeffPre * srFactor)) + (x - sx1);
                x = x + (x * color.preEmphasisGain);

                // Base signal selection
                const baseSig = (color.flagA !== 0) ? x : inSample;

                // Type 4 scaling
                if (isType4 !== 0) {
                    x = x * scaledProcess;
                }

                // 1st PHXTBLDATA lookup
                x = lookupPhxTbl(x, color.tableOffset);

                // Saturation mix + baseSig
                x = (x * scaledProcess * color.satMixGain) + baseSig;

                // 2nd PHXTBLDATA lookup
                x = lookupPhxTbl(x, color.tableOffset);

                // De-emphasis / reconstruction filter
                x = ((x - prevY1) * (coeffPost * srFactor)) + prevY1;
                sy1 = x;

                // Dry dynamic compensation & output trim
                const dryComp = inSample + (-1.0 * effProcess * inSample * color.dryCompGain);
                const wetSum = (x * effProcess) + dryComp;
                const outSample = wetSum * outTrimGain;

                dst[i] = outSample;
                sx1 = inSample;

                if (Math.abs(outSample) > outPeak) outPeak = Math.abs(outSample);
                satEnergy += Math.abs(x * effProcess);
            }

            sxArr[ch] = sx1;
            syArr[ch] = sy1;
        }

        filterState.x1L = sxArr[0];
        filterState.y1L = syArr[0];
        filterState.x1R = sxArr[1];
        filterState.y1R = syArr[1];

        state.meters.inPeak = inPeak;
        state.meters.outPeak = outPeak;
        state.meters.driveAmt = Math.min(100.0, (satEnergy / (numSamples * 2)) * 260.0 + p.process * 0.65);
    }

    // ========================================================================
    // 2. UI & 7-SKIN RENDERER (1:1 Sprites + Custom Vector/Pixel Knobs per Skin)
    // ========================================================================
    const SKIN_META = {
        cranesong: {
            icon: "🎚️",
            title: "Crane Song Phoenix II — 1:1 Hardware & AAX Edition [Developer: papielguapo]",
            host: "AAX / TDM / VST3 64-BIT",
            subL: "MANID: CRAN • PRODID: Phx2 • PLUGID: Phx2 / Px2R / Px2T",
            subR: "DESIGNED BY DAVID HILL • CLONED 1:1 BY PAPIELGUAPO"
        },
        flstudio: {
            icon: "🍓",
            title: "Fruity Wrapper — Phoenix II (by papielguapo) [FL Studio 21]",
            host: "FL STUDIO 64-BIT • INSERT 1",
            subL: "IMAGE-LINE VECTOR ENGINE • CREATOR: papielguapo",
            subR: "HQ OVERSAMPLING • 24,576-PT WAVESHAPER"
        },
        protools: {
            icon: "🎛️",
            title: "Insert A: Phoenix II (Stereo) — Track: Master Bus [papielguapo]",
            host: "AVID PRO TOOLS HDX / NATIVE",
            subL: "AUTO: SAFE • TARGET: ON • COMPARE • BYPASS",
            subR: "TI C67x / x64 ALGORITHM • DEVELOPER: papielguapo"
        },
        ableton: {
            icon: "🟡",
            title: "Phoenix II Tape Device — Ableton Live 12 [papielguapo]",
            host: "ABLETON LIVE DEVICE RACK",
            subL: "AUDIO EFFECT RACK • CHAIN 1 • 0.0 MS LATENCY",
            subR: "ANALOG TAPE SATURATION • DEV: papielguapo"
        },
        logicpro: {
            icon: "🍎",
            title: "Phoenix II Tape Console — Logic Pro 11 Audio Units [papielguapo]",
            host: "AU / LOGIC PRO STEREO",
            subL: "STEREO OUT • SLOT 1 • DUAL PRECISION 64-BIT",
            subR: "HARMONIC MAGNETIZATION MODEL • DEV: papielguapo"
        },
        win98: {
            icon: "💻",
            title: "Phoenix98.exe - Crane Song Phoenix II [Creador: papielguapo]",
            host: "WIN98 DIRECTX AUDIO PLUG-IN",
            subL: "File   Edit   Skins   Presets   DirectSound   Help",
            subR: "Microsoft Windows 98 SE • Developer: papielguapo"
        },
        ataripc: {
            icon: "🕹️",
            title: "PHOENIX2.PRG — ATARI ST GEM / TOS DSP MONITOR [PAPIELGUAPO]",
            host: "GEM TOS 1.04 • 68000 / DSP56001",
            subL: "ADDR: $008020 PHXTBLDATA (98,304 BYTES)",
            subR: "PHOSPHOR P1 CRT • DEVELOPER: PAPIELGUAPO"
        }
    };

    function initOriginalSprites() {
        const assets = window.PHOENIX_ORIGINAL_ASSETS;
        if (!assets) return;

        const fp = document.getElementById("faceplateBg");
        if (fp) fp.style.backgroundImage = `url("${assets.faceplate}")`;

        ["spriteInputTrim", "spriteProcess", "spriteOutputTrim"].forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.style.backgroundImage = `url("${assets.knob_cont}")`;
                el.style.backgroundSize = "60px 8100px";
            }
        });

        const sb = document.getElementById("spriteBrightness");
        if (sb) {
            sb.style.backgroundImage = `url("${assets.knob_bright}")`;
            sb.style.backgroundSize = "60px 180px";
        }

        const st = document.getElementById("spriteType");
        if (st) {
            st.style.backgroundImage = `url("${assets.knob_type}")`;
            st.style.backgroundSize = "60px 300px";
        }
    }

    function getParamNormalized(param) {
        const p = state.params;
        if (param === "inputTrim") return (p.inputTrim + 10.0) / 20.0;
        if (param === "process") return p.process / 100.0;
        if (param === "outputTrim") return (p.outputTrim + 6.0) / 12.0;
        if (param === "brightness") return (p.brightness - 1) / 2.0;
        if (param === "type") return (p.type - 1) / 4.0;
        return 0.5;
    }

    function setParamFromNormalized(param, norm) {
        const n = Math.max(0.0, Math.min(1.0, norm));
        if (param === "inputTrim") {
            state.params.inputTrim = Math.round((-10.0 + n * 20.0) * 10) / 10;
        } else if (param === "process") {
            state.params.process = Math.round((n * 100.0) * 10) / 10;
        } else if (param === "outputTrim") {
            state.params.outputTrim = Math.round((-6.0 + n * 12.0) * 10) / 10;
        } else if (param === "brightness") {
            state.params.brightness = Math.max(1, Math.min(3, Math.round(1 + n * 2)));
        } else if (param === "type") {
            state.params.type = Math.max(1, Math.min(5, Math.round(1 + n * 4)));
        }
        syncSlotFromCurrent();
        updateAllUI();
    }

    function resetSingleParam(param) {
        if (param === "inputTrim") state.params.inputTrim = 0.0;
        else if (param === "process") state.params.process = 0.0;
        else if (param === "outputTrim") state.params.outputTrim = 0.0;
        else if (param === "brightness") state.params.brightness = 2;
        else if (param === "type") state.params.type = 1;
        syncSlotFromCurrent();
        updateAllUI();
    }

    function syncSlotFromCurrent() {
        const target = state.activeSlot === "A" ? state.slotA : state.slotB;
        Object.assign(target, state.params);
    }

    // Draw custom vector / pixel-art knob on canvas according to active skin
    function drawSkinnedKnob(canvas, param) {
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const w = canvas.width;
        const h = canvas.height;
        const cx = w / 2;
        const cy = h / 2;
        ctx.clearRect(0, 0, w, h);

        const norm = getParamNormalized(param);
        const startAng = Math.PI * 0.75;
        const endAng = Math.PI * 2.25;
        const valAng = startAng + norm * (endAng - startAng);
        const skin = state.skin;

        if (skin === "flstudio") {
            ctx.beginPath();
            ctx.arc(cx, cy, 40, startAng, endAng);
            ctx.strokeStyle = "#14171c";
            ctx.lineWidth = 7;
            ctx.lineCap = "round";
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 40, startAng, valAng);
            ctx.strokeStyle = param === "process" ? "#00e5ff" : "#ff9100";
            ctx.lineWidth = 7;
            ctx.lineCap = "round";
            ctx.stroke();

            const grad = ctx.createRadialGradient(cx - 6, cy - 8, 4, cx, cy, 31);
            grad.addColorStop(0, "#3d4450");
            grad.addColorStop(1, "#1c2026");
            ctx.beginPath();
            ctx.arc(cx, cy, 30, 0, Math.PI * 2);
            ctx.fillStyle = grad;
            ctx.fill();
            ctx.lineWidth = 2;
            ctx.strokeStyle = "#111317";
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(cx + Math.cos(valAng) * 10, cy + Math.sin(valAng) * 10);
            ctx.lineTo(cx + Math.cos(valAng) * 27, cy + Math.sin(valAng) * 27);
            ctx.strokeStyle = "#ffffff";
            ctx.lineWidth = 2.6;
            ctx.stroke();

        } else if (skin === "protools") {
            const numDots = 15;
            for (let i = 0; i < numDots; i++) {
                const t = i / (numDots - 1);
                const a = startAng + t * (endAng - startAng);
                const dx = cx + Math.cos(a) * 42;
                const dy = cy + Math.sin(a) * 42;
                ctx.beginPath();
                ctx.arc(dx, dy, 2.7, 0, Math.PI * 2);
                ctx.fillStyle = (t <= norm + 0.01) ? (t > 0.8 ? "#ffae00" : "#39ff14") : "#191c19";
                ctx.fill();
            }

            const grad = ctx.createRadialGradient(cx - 8, cy - 8, 3, cx, cy, 32);
            grad.addColorStop(0, "#9ea0a8");
            grad.addColorStop(0.7, "#4d4f57");
            grad.addColorStop(1, "#232428");
            ctx.beginPath();
            ctx.arc(cx, cy, 31, 0, Math.PI * 2);
            ctx.fillStyle = grad;
            ctx.fill();
            ctx.lineWidth = 2;
            ctx.strokeStyle = "#18181b";
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(cx + Math.cos(valAng) * 8, cy + Math.sin(valAng) * 8);
            ctx.lineTo(cx + Math.cos(valAng) * 28, cy + Math.sin(valAng) * 28);
            ctx.strokeStyle = "#111";
            ctx.lineWidth = 3.2;
            ctx.stroke();

        } else if (skin === "ableton") {
            ctx.beginPath();
            ctx.arc(cx, cy, 37, startAng, endAng);
            ctx.strokeStyle = "#181818";
            ctx.lineWidth = 6;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 37, startAng, valAng);
            ctx.strokeStyle = param === "process" ? "#00d8ff" : "#ffae00";
            ctx.lineWidth = 6;
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(valAng) * 37, cy + Math.sin(valAng) * 37);
            ctx.strokeStyle = "#eeeeee";
            ctx.lineWidth = 2.5;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 4, 0, Math.PI * 2);
            ctx.fillStyle = "#eeeeee";
            ctx.fill();

        } else if (skin === "logicpro") {
            ctx.beginPath();
            ctx.arc(cx, cy, 41, startAng, endAng);
            ctx.strokeStyle = "#10131c";
            ctx.lineWidth = 5;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 41, startAng, valAng);
            ctx.strokeStyle = param === "process" ? "#a29bfe" : "#00d2ff";
            ctx.lineWidth = 5;
            ctx.stroke();

            const grad = ctx.createLinearGradient(cx - 30, cy - 30, cx + 30, cy + 30);
            grad.addColorStop(0, "#3b4259");
            grad.addColorStop(0.5, "#232838");
            grad.addColorStop(1, "#12151f");
            ctx.beginPath();
            ctx.arc(cx, cy, 31, 0, Math.PI * 2);
            ctx.fillStyle = grad;
            ctx.fill();
            ctx.strokeStyle = "#4c5675";
            ctx.lineWidth = 1.5;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx + Math.cos(valAng) * 21, cy + Math.sin(valAng) * 21, 3.5, 0, Math.PI * 2);
            ctx.fillStyle = "#00d2ff";
            ctx.fill();

        } else if (skin === "win98") {
            for (let i = 0; i <= 10; i++) {
                const a = startAng + (i / 10) * (endAng - startAng);
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(a) * 36, cy + Math.sin(a) * 36);
                ctx.lineTo(cx + Math.cos(a) * 43, cy + Math.sin(a) * 43);
                ctx.strokeStyle = "#000000";
                ctx.lineWidth = 1.5;
                ctx.stroke();
            }

            ctx.beginPath();
            ctx.arc(cx, cy, 31, 0, Math.PI * 2);
            ctx.fillStyle = "#c0c0c0";
            ctx.fill();

            ctx.beginPath();
            ctx.arc(cx, cy, 31, Math.PI * 0.75, Math.PI * 1.75);
            ctx.strokeStyle = "#ffffff";
            ctx.lineWidth = 3;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 31, Math.PI * 1.75, Math.PI * 0.75);
            ctx.strokeStyle = "#404040";
            ctx.lineWidth = 3;
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(valAng) * 25, cy + Math.sin(valAng) * 25);
            ctx.strokeStyle = "#000080";
            ctx.lineWidth = 3;
            ctx.stroke();

        } else if (skin === "ataripc") {
            ctx.strokeStyle = "#33ff66";
            ctx.lineWidth = 2;
            ctx.beginPath();
            const sides = 8;
            for (let i = 0; i <= sides; i++) {
                const a = (i / sides) * Math.PI * 2 + Math.PI / 8;
                const rx = cx + Math.cos(a) * 33;
                const ry = cy + Math.sin(a) * 33;
                if (i === 0) ctx.moveTo(rx, ry);
                else ctx.lineTo(rx, ry);
            }
            ctx.stroke();

            for (let i = 0; i <= 12; i++) {
                const t = i / 12;
                const a = startAng + t * (endAng - startAng);
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(a) * 38, cy + Math.sin(a) * 38);
                ctx.lineTo(cx + Math.cos(a) * 44, cy + Math.sin(a) * 44);
                ctx.strokeStyle = t <= norm + 0.01 ? "#33ff66" : "#145222";
                ctx.lineWidth = 2;
                ctx.stroke();
            }

            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(valAng) * 28, cy + Math.sin(valAng) * 28);
            ctx.strokeStyle = "#33ff66";
            ctx.lineWidth = 3;
            ctx.stroke();

        } else {
            ctx.beginPath();
            ctx.arc(cx, cy, 41, startAng, endAng);
            ctx.strokeStyle = "#1a2824";
            ctx.lineWidth = 5;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(cx, cy, 41, startAng, valAng);
            ctx.strokeStyle = "#38c998";
            ctx.lineWidth = 5;
            ctx.stroke();

            const grad = ctx.createRadialGradient(cx - 7, cy - 7, 3, cx, cy, 32);
            grad.addColorStop(0, "#3b8f74");
            grad.addColorStop(0.75, "#155742");
            grad.addColorStop(1, "#0b2e22");
            ctx.beginPath();
            ctx.arc(cx, cy, 31, 0, Math.PI * 2);
            ctx.fillStyle = grad;
            ctx.fill();
            ctx.lineWidth = 2.5;
            ctx.strokeStyle = "#081712";
            ctx.stroke();

            ctx.beginPath();
            ctx.moveTo(cx + Math.cos(valAng) * 14, cy + Math.sin(valAng) * 14);
            ctx.lineTo(cx + Math.cos(valAng) * 28, cy + Math.sin(valAng) * 28);
            ctx.strokeStyle = "#e8fff7";
            ctx.lineWidth = 3;
            ctx.lineCap = "round";
            ctx.stroke();
        }
    }

    function updateAllUI() {
        const p = state.params;

        const inFrame = Math.round(getParamNormalized("inputTrim") * 134);
        const procFrame = Math.round(getParamNormalized("process") * 134);
        const outFrame = Math.round(getParamNormalized("outputTrim") * 134);
        const brtFrame = Math.max(0, Math.min(2, p.brightness - 1));
        const typFrame = Math.max(0, Math.min(4, p.type - 1));

        const sIn = document.getElementById("spriteInputTrim");
        const sPr = document.getElementById("spriteProcess");
        const sOu = document.getElementById("spriteOutputTrim");
        const sBr = document.getElementById("spriteBrightness");
        const sTy = document.getElementById("spriteType");

        if (sIn) sIn.style.backgroundPosition = `0px -${inFrame * 60}px`;
        if (sPr) sPr.style.backgroundPosition = `0px -${procFrame * 60}px`;
        if (sOu) sOu.style.backgroundPosition = `0px -${outFrame * 60}px`;
        if (sBr) sBr.style.backgroundPosition = `0px -${brtFrame * 60}px`;
        if (sTy) sTy.style.backgroundPosition = `0px -${typFrame * 60}px`;

        const inStr = `${p.inputTrim >= 0 ? "+" : ""}${p.inputTrim.toFixed(1)} dB`;
        const prStr = `${p.process.toFixed(1)} %`;
        const ouStr = `${p.outputTrim >= 0 ? "+" : ""}${p.outputTrim.toFixed(1)} dB`;
        const brStr = BRIGHTNESS_NAMES[brtFrame];
        const tyObj = PHOENIX_COLOR_TYPES[typFrame];

        document.getElementById("roInputTrim").textContent = p.inputTrim.toFixed(2);
        document.getElementById("roProcess").textContent = p.process.toFixed(2);
        document.getElementById("roOutputTrim").textContent = p.outputTrim.toFixed(2);
        document.getElementById("roBrightness").textContent = brStr;
        document.getElementById("roType").textContent = tyObj.shortName;

        document.getElementById("valInputTrim").textContent = inStr;
        document.getElementById("valProcess").textContent = prStr;
        document.getElementById("valOutputTrim").textContent = ouStr;
        document.getElementById("valBrightness").textContent = brStr;
        document.getElementById("valType").textContent = tyObj.name;

        document.querySelectorAll("[data-set-bright]").forEach(btn => {
            btn.classList.toggle("active", parseInt(btn.getAttribute("data-set-bright"), 10) === p.brightness);
        });
        document.querySelectorAll("[data-set-type]").forEach(btn => {
            btn.classList.toggle("active", parseInt(btn.getAttribute("data-set-type"), 10) === p.type);
        });

        const bypBtn = document.getElementById("btnMasterBypass");
        if (bypBtn) {
            bypBtn.textContent = p.bypass ? "MASTER BYPASS: ON" : "MASTER BYPASS: OFF";
            bypBtn.classList.toggle("bypass-active", p.bypass);
        }

        drawSkinnedKnob(document.getElementById("cvInputTrim"), "inputTrim");
        drawSkinnedKnob(document.getElementById("cvProcess"), "process");
        drawSkinnedKnob(document.getElementById("cvOutputTrim"), "outputTrim");
        drawSkinnedKnob(document.getElementById("cvBrightness"), "brightness");
        drawSkinnedKnob(document.getElementById("cvType"), "type");

        const cPre = tyObj.brightCoeffs[brtFrame][0];
        const cPost = tyObj.brightCoeffs[brtFrame][1];
        document.getElementById("dspSpecCoeffs").textContent =
            `Pre: ${cPre.toFixed(4)} | Post: ${cPost.toFixed(4)} | DriveScale: ${tyObj.driveScale.toFixed(4)} | SatMix: ${tyObj.satMixGain.toFixed(4)}`;
        document.getElementById("dspSpecTable").textContent =
            `PHXTBL Offset: ${tyObj.tableOffset} | FlagA: ${tyObj.flagA} | Developer: papielguapo`;
        document.getElementById("curveInfoBadge").textContent =
            `Table ${tyObj.tableOffset / 8192} • ${brStr} • ${tyObj.name}`;

        drawTransferCurve();
    }

    function drawTransferCurve() {
        const cv = document.getElementById("cvTransferCurve");
        if (!cv) return;
        const ctx = cv.getContext("2d");
        const w = cv.width;
        const h = cv.height;
        ctx.clearRect(0, 0, w, h);

        ctx.strokeStyle = state.skin === "ataripc" ? "#10451b" : "#1e2631";
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2);
        ctx.moveTo(w / 2, 0); ctx.lineTo(w / 2, h);
        ctx.stroke();

        ctx.strokeStyle = "rgba(150,160,175,0.22)";
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.moveTo(0, h); ctx.lineTo(w, 0);
        ctx.stroke();
        ctx.setLineDash([]);

        const p = state.params;
        const tIdx = Math.max(1, Math.min(5, p.type));
        const color = PHOENIX_COLOR_TYPES[tIdx - 1];
        const processVal = p.process / 100.0;
        const isType4 = (tIdx === 4) ? 1 : 0;
        const type4Factor = 1.0 - isType4 * 0.5;
        const scaledProcess = processVal * color.driveScale;
        const effProcess = scaledProcess * type4Factor;

        ctx.strokeStyle = state.skin === "ataripc" ? "rgba(51,255,102,0.45)" : "rgba(0, 210, 255, 0.45)";
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (let px = 0; px < w; px++) {
            const xVal = (px / (w - 1)) * 2.0 - 1.0;
            const tblY = lookupPhxTbl(xVal, color.tableOffset);
            const py = (h / 2) - (tblY * (h * 0.45));
            if (px === 0) ctx.moveTo(px, py);
            else ctx.lineTo(px, py);
        }
        ctx.stroke();

        ctx.strokeStyle = state.skin === "ataripc" ? "#33ff66" : (state.skin === "flstudio" ? "#ff9100" : "#38e5a5");
        ctx.lineWidth = 2.2;
        ctx.beginPath();
        for (let px = 0; px < w; px++) {
            const inSample = (px / (w - 1)) * 2.0 - 1.0;
            let x = inSample * (1.0 + color.preEmphasisGain * 0.5);
            const baseSig = (color.flagA !== 0) ? x : inSample;
            if (isType4 !== 0) x = x * scaledProcess;
            x = lookupPhxTbl(x, color.tableOffset);
            x = (x * scaledProcess * color.satMixGain) + baseSig;
            x = lookupPhxTbl(x, color.tableOffset);
            const dryComp = inSample + (-1.0 * effProcess * inSample * color.dryCompGain);
            const outSample = p.bypass ? inSample : ((x * effProcess) + dryComp);
            const py = (h / 2) - (Math.max(-1.15, Math.min(1.15, outSample)) * (h * 0.42));
            if (px === 0) ctx.moveTo(px, py);
            else ctx.lineTo(px, py);
        }
        ctx.stroke();
    }

    function applySkin(skinId) {
        state.skin = skinId;
        document.body.setAttribute("data-skin", skinId);
        document.querySelectorAll(".skin-btn").forEach(btn => {
            btn.classList.toggle("active", btn.getAttribute("data-skin-target") === skinId);
        });

        const meta = SKIN_META[skinId] || SKIN_META.cranesong;
        document.getElementById("deckWinIcon").textContent = meta.icon;
        document.getElementById("deckWinTitle").textContent = meta.title;
        document.getElementById("deckHostTag").textContent = meta.host;
        document.getElementById("deckSubLeft").textContent = meta.subL;
        document.getElementById("deckSubRight").textContent = meta.subR;

        const nativeFrame = document.getElementById("nativeHardwareFrame");
        const skinnedDeck = document.getElementById("skinnedPluginDeck");
        if (state.dualView) {
            nativeFrame.style.display = "flex";
            skinnedDeck.style.display = "block";
        } else {
            if (skinId === "cranesong") {
                nativeFrame.style.display = "flex";
                skinnedDeck.style.display = "none";
            } else {
                nativeFrame.style.display = "none";
                skinnedDeck.style.display = "block";
            }
        }

        updateAllUI();
    }

    function bindInteractions() {
        document.querySelectorAll(".skin-btn").forEach(btn => {
            btn.addEventListener("click", () => {
                applySkin(btn.getAttribute("data-skin-target"));
            });
        });

        const scaleSel = document.getElementById("scaleSelect");
        const scaleWrapper = document.getElementById("scaleWrapper");
        function updateScale(sc) {
            state.scale = sc;
            document.documentElement.style.setProperty("--plugin-scale", sc);
            const padY = Math.max(8, Math.round((161 * sc - 161) / 2 + 10));
            scaleWrapper.style.padding = `${padY}px 12px`;
        }
        scaleSel.addEventListener("change", () => updateScale(parseFloat(scaleSel.value)));
        updateScale(1.5);

        const dualBtn = document.getElementById("btnToggleHardwareOverlay");
        dualBtn.textContent = state.dualView ? "Vista Dual: ON" : "Vista Dual: OFF";
        dualBtn.addEventListener("click", () => {
            state.dualView = !state.dualView;
            dualBtn.textContent = state.dualView ? "Vista Dual: ON" : "Vista Dual: OFF";
            applySkin(state.skin);
        });

        const csLogo = document.getElementById("craneSongLogoHotspot");
        if (csLogo) {
            csLogo.addEventListener("dblclick", (e) => {
                e.preventDefault();
                if (state.skin === "cranesong" && !state.dualView) {
                    applySkin("win98");
                } else {
                    state.dualView = false;
                    dualBtn.textContent = "Vista Dual: OFF";
                    applySkin("cranesong");
                }
            });
        }

        const roBrt = document.getElementById("roBrightness");
        if (roBrt) {
            roBrt.addEventListener("click", () => {
                state.params.brightness = (state.params.brightness % 3) + 1;
                syncSlotFromCurrent();
                updateAllUI();
            });
        }
        const roTyp = document.getElementById("roType");
        if (roTyp) {
            roTyp.addEventListener("click", () => {
                state.params.type = (state.params.type % 5) + 1;
                syncSlotFromCurrent();
                updateAllUI();
            });
        }

        document.getElementById("btnMasterBypass").addEventListener("click", () => {
            state.params.bypass = !state.params.bypass;
            syncSlotFromCurrent();
            updateAllUI();
        });

        document.getElementById("presetSelect").addEventListener("change", (e) => {
            const pr = PRESETS[e.target.value];
            if (pr) {
                Object.assign(state.params, pr);
                syncSlotFromCurrent();
                updateAllUI();
            }
        });

        const btnA = document.getElementById("btnSlotA");
        const btnB = document.getElementById("btnSlotB");
        const btnCopy = document.getElementById("btnCopyAB");
        btnA.addEventListener("click", () => {
            state.activeSlot = "A";
            Object.assign(state.params, state.slotA);
            btnA.classList.add("ab-active");
            btnB.classList.remove("ab-active");
            btnCopy.textContent = "A → B";
            updateAllUI();
        });
        btnB.addEventListener("click", () => {
            state.activeSlot = "B";
            Object.assign(state.params, state.slotB);
            btnB.classList.add("ab-active");
            btnA.classList.remove("ab-active");
            btnCopy.textContent = "B → A";
            updateAllUI();
        });
        btnCopy.addEventListener("click", () => {
            if (state.activeSlot === "A") Object.assign(state.slotB, state.params);
            else Object.assign(state.slotA, state.params);
        });

        document.getElementById("btnResetParams").addEventListener("click", () => {
            Object.assign(state.params, PRESETS.default, { bypass: false });
            syncSlotFromCurrent();
            updateAllUI();
        });

        document.querySelectorAll("[data-set-bright]").forEach(el => {
            el.addEventListener("click", () => {
                state.params.brightness = parseInt(el.getAttribute("data-set-bright"), 10);
                syncSlotFromCurrent();
                updateAllUI();
            });
        });
        document.querySelectorAll("[data-set-type]").forEach(el => {
            el.addEventListener("click", () => {
                state.params.type = parseInt(el.getAttribute("data-set-type"), 10);
                syncSlotFromCurrent();
                updateAllUI();
            });
        });

        let dragTarget = null;
        let dragStartY = 0;
        let dragStartNorm = 0;

        document.querySelectorAll("[data-param]").forEach(el => {
            const param = el.getAttribute("data-param");

            el.addEventListener("mousedown", (e) => {
                e.preventDefault();
                dragTarget = param;
                dragStartY = e.clientY;
                dragStartNorm = getParamNormalized(param);
            });

            el.addEventListener("dblclick", (e) => {
                e.preventDefault();
                resetSingleParam(param);
            });

            el.addEventListener("wheel", (e) => {
                e.preventDefault();
                const step = (param === "brightness") ? 0.5 : (param === "type" ? 0.25 : (e.shiftKey ? 0.005 : 0.025));
                const dir = e.deltaY < 0 ? 1 : -1;
                setParamFromNormalized(param, getParamNormalized(param) + dir * step);
            }, { passive: false });
        });

        window.addEventListener("mousemove", (e) => {
            if (!dragTarget) return;
            const dy = dragStartY - e.clientY;
            const sens = e.shiftKey ? 450.0 : 150.0;
            setParamFromNormalized(dragTarget, dragStartNorm + dy / sens);
        });

        window.addEventListener("mouseup", () => {
            dragTarget = null;
        });
    }

    // ========================================================================
    // 3. REAL-TIME WEB AUDIO ENGINE, SYNTH GENERATOR, MIC & WAV EXPORT
    // ========================================================================
    let audioCtx = null;
    let scriptNode = null;
    let analyserNode = null;
    let isPlaying = false;
    let synthPhase = 0;
    let customBuffer = null;
    let customPlayHead = 0;
    let micStream = null;
    let micSource = null;
    let useMicInput = false;

    function generateProceduralSample(mode, t, dt) {
        if (mode === "sine1k") {
            const s = Math.sin(2.0 * Math.PI * 1000.0 * t) * 0.72;
            return [s, s];
        }

        const bpm = 116.0;
        const beat = (t * (bpm / 60.0));
        const step = Math.floor(beat * 4) % 16;
        const stepFrac = (beat * 4) - Math.floor(beat * 4);
        const stepTime = stepFrac * (60.0 / bpm / 4.0);

        let kick = 0;
        if (step === 0 || step === 6 || step === 8 || step === 14) {
            const kFreq = 48.0 + 130.0 * Math.exp(-stepTime * 38.0);
            kick = Math.sin(2 * Math.PI * kFreq * stepTime) * Math.exp(-stepTime * 11.0) * 0.85;
        }

        let snare = 0;
        if (step === 4 || step === 12) {
            const noise = (Math.random() * 2 - 1) * Math.exp(-stepTime * 22.0);
            const body = Math.sin(2 * Math.PI * 185.0 * stepTime) * Math.exp(-stepTime * 26.0);
            snare = (noise * 0.55 + body * 0.45) * 0.7;
        }
        const hatEnv = Math.exp(-stepTime * (step % 2 === 1 ? 45.0 : 75.0));
        const hat = (Math.random() * 2 - 1) * hatEnv * 0.18;

        const bassNotes = [55.0, 55.0, 65.41, 55.0, 73.42, 65.41, 49.0, 55.0];
        const bFreq = bassNotes[Math.floor(beat / 0.5) % bassNotes.length];
        const bEnv = Math.exp(-(beat % 0.5) * 5.5);
        const bassRaw = (Math.sin(2 * Math.PI * bFreq * t) + 0.45 * Math.sin(2 * Math.PI * bFreq * 2 * t)) * bEnv * 0.55;

        const chordProgression = [
            [220.0, 261.63, 329.63, 392.0],
            [196.0, 246.94, 293.66, 349.23],
            [174.61, 220.0, 261.63, 329.63],
            [164.81, 196.0, 246.94, 329.63]
        ];
        const chord = chordProgression[Math.floor(beat / 4) % 4];
        let chordsL = 0, chordsR = 0;
        for (let i = 0; i < chord.length; i++) {
            const f = chord[i];
            const v = Math.sin(2 * Math.PI * f * t + 0.3 * Math.sin(2 * Math.PI * f * 2 * t)) * 0.14;
            chordsL += v * (0.7 + 0.1 * i);
            chordsR += v * (1.0 - 0.1 * i);
        }

        if (mode === "drums") return [kick + snare + hat * 1.1, kick + snare * 0.95 + hat * 0.85];
        if (mode === "bass") return [bassRaw, bassRaw];
        if (mode === "chords") return [chordsL * 1.6, chordsR * 1.6];

        const mixL = (kick + snare + hat + bassRaw * 0.8 + chordsL) * 0.72;
        const mixR = (kick + snare * 0.96 + hat * 0.85 + bassRaw * 0.8 + chordsR) * 0.72;
        return [mixL, mixR];
    }

    function ensureAudioEngine() {
        if (audioCtx) return;
        const AC = window.AudioContext || window.webkitAudioContext;
        audioCtx = new AC();
        analyserNode = audioCtx.createAnalyser();
        analyserNode.fftSize = 1024;
        analyserNode.smoothingTimeConstant = 0.78;

        scriptNode = audioCtx.createScriptProcessor(1024, 2, 2);
        const sr = audioCtx.sampleRate;
        const inL = new Float32Array(1024);
        const inR = new Float32Array(1024);

        document.getElementById("audioStatusBadge").textContent = `DSP ACTIVO (${(sr / 1000).toFixed(1)} kHz)`;

        scriptNode.onaudioprocess = function (e) {
            const outL = e.outputBuffer.getChannelData(0);
            const outR = e.outputBuffer.getChannelData(1);
            const n = outL.length;

            if (!isPlaying && !useMicInput) {
                for (let i = 0; i < n; i++) { outL[i] = 0; outR[i] = 0; }
                state.meters.inPeak *= 0.85;
                state.meters.outPeak *= 0.85;
                state.meters.driveAmt *= 0.85;
                return;
            }

            const mode = document.getElementById("audioSourceSelect").value;
            if (useMicInput) {
                const micInL = e.inputBuffer.getChannelData(0);
                const micInR = e.inputBuffer.numberOfChannels > 1 ? e.inputBuffer.getChannelData(1) : micInL;
                for (let i = 0; i < n; i++) { inL[i] = micInL[i]; inR[i] = micInR[i]; }
            } else if (mode === "custom" && customBuffer) {
                const cL = customBuffer.getChannelData(0);
                const cR = customBuffer.numberOfChannels > 1 ? customBuffer.getChannelData(1) : cL;
                const len = customBuffer.length;
                for (let i = 0; i < n; i++) {
                    inL[i] = cL[customPlayHead];
                    inR[i] = cR[customPlayHead];
                    customPlayHead = (customPlayHead + 1) % len;
                }
            } else {
                const dt = 1.0 / sr;
                for (let i = 0; i < n; i++) {
                    const pair = generateProceduralSample(mode, synthPhase, dt);
                    inL[i] = pair[0];
                    inR[i] = pair[1];
                    synthPhase += dt;
                }
            }

            processPhoenixBlock(inL, inR, outL, outR, n, sr, state.dspState);
        };

        scriptNode.connect(analyserNode);
        analyserNode.connect(audioCtx.destination);
    }

    function bindAudioControls() {
        const playBtn = document.getElementById("btnPlayAudio");
        playBtn.addEventListener("click", async () => {
            ensureAudioEngine();
            if (audioCtx.state === "suspended") await audioCtx.resume();
            isPlaying = !isPlaying;
            playBtn.textContent = isPlaying ? "⏹ Detener Audio DSP" : "▶ Reproducir Audio DSP";
            playBtn.classList.toggle("playing", isPlaying);
        });

        const fileInput = document.getElementById("audioFileInput");
        fileInput.addEventListener("change", async (e) => {
            const file = e.target.files[0];
            if (!file) return;
            ensureAudioEngine();
            const arr = await file.arrayBuffer();
            customBuffer = await audioCtx.decodeAudioData(arr);
            customPlayHead = 0;
            document.getElementById("audioSourceSelect").value = "custom";
            if (!isPlaying) playBtn.click();
        });

        const micBtn = document.getElementById("btnMicInput");
        micBtn.addEventListener("click", async () => {
            ensureAudioEngine();
            if (audioCtx.state === "suspended") await audioCtx.resume();
            if (!useMicInput) {
                try {
                    micStream = await navigator.mediaDevices.getUserMedia({ audio: true });
                    micSource = audioCtx.createMediaStreamSource(micStream);
                    micSource.connect(scriptNode);
                    useMicInput = true;
                    micBtn.classList.add("bypass-active");
                    micBtn.textContent = "🎙️ Live: ON";
                } catch (err) {
                    alert("No se pudo acceder a la entrada de audio: " + err.message);
                }
            } else {
                useMicInput = false;
                if (micSource) micSource.disconnect();
                if (micStream) micStream.getTracks().forEach(t => t.stop());
                micBtn.classList.remove("bypass-active");
                micBtn.textContent = "🎙️ Entrada Live";
            }
        });

        document.getElementById("btnExportWav").addEventListener("click", () => {
            const sr = 48000;
            const mode = document.getElementById("audioSourceSelect").value;
            let numSamples = sr * 4;
            let srcL, srcR;

            if (mode === "custom" && customBuffer) {
                numSamples = customBuffer.length;
                srcL = new Float32Array(customBuffer.getChannelData(0));
                srcR = new Float32Array(customBuffer.numberOfChannels > 1 ? customBuffer.getChannelData(1) : srcL);
            } else {
                srcL = new Float32Array(numSamples);
                srcR = new Float32Array(numSamples);
                const dt = 1.0 / sr;
                for (let i = 0; i < numSamples; i++) {
                    const pair = generateProceduralSample(mode, i * dt, dt);
                    srcL[i] = pair[0];
                    srcR[i] = pair[1];
                }
            }

            const outL = new Float32Array(numSamples);
            const outR = new Float32Array(numSamples);
            const offlineFilter = { x1L: 0, y1L: 0, x1R: 0, y1R: 0 };
            processPhoenixBlock(srcL, srcR, outL, outR, numSamples, sr, offlineFilter);

            const wavBytes = encodeStereoWav16(outL, outR, sr);
            const blob = new Blob([wavBytes], { type: "audio/wav" });
            const url = URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            a.download = `PhoenixII_papielguapo_${PHOENIX_COLOR_TYPES[state.params.type - 1].shortName}.wav`;
            a.click();
            URL.revokeObjectURL(url);
        });
    }

    function encodeStereoWav16(left, right, sampleRate) {
        const numSamples = left.length;
        const buffer = new ArrayBuffer(44 + numSamples * 4);
        const view = new DataView(buffer);
        function writeStr(ofs, s) {
            for (let i = 0; i < s.length; i++) view.setUint8(ofs + i, s.charCodeAt(i));
        }
        writeStr(0, "RIFF");
        view.setUint32(4, 36 + numSamples * 4, true);
        writeStr(8, "WAVE");
        writeStr(12, "fmt ");
        view.setUint32(16, 16, true);
        view.setUint16(20, 1, true);
        view.setUint16(22, 2, true);
        view.setUint32(24, sampleRate, true);
        view.setUint32(28, sampleRate * 4, true);
        view.setUint16(32, 4, true);
        view.setUint16(34, 16, true);
        writeStr(36, "data");
        view.setUint32(40, numSamples * 4, true);

        let offset = 44;
        for (let i = 0; i < numSamples; i++) {
            const sL = Math.max(-1, Math.min(1, left[i]));
            const sR = Math.max(-1, Math.min(1, right[i]));
            view.setInt16(offset, sL < 0 ? sL * 0x8000 : sL * 0x7FFF, true);
            view.setInt16(offset + 2, sR < 0 ? sR * 0x8000 : sR * 0x7FFF, true);
            offset += 4;
        }
        return buffer;
    }

    function startVisualizerLoop() {
        const cvScope = document.getElementById("cvScopeSpectrum");
        const ctx = cvScope.getContext("2d");
        const freqData = new Uint8Array(512);
        const timeData = new Uint8Array(512);

        function tick() {
            const inP = state.meters.inPeak;
            const outP = state.meters.outPeak;
            const drv = state.meters.driveAmt;

            const inDb = inP > 0.0001 ? (20 * Math.log10(inP)) : -90;
            const outDb = outP > 0.0001 ? (20 * Math.log10(outP)) : -90;
            const inPct = Math.max(0, Math.min(100, ((inDb + 48) / 54) * 100));
            const outPct = Math.max(0, Math.min(100, ((outDb + 48) / 54) * 100));

            document.getElementById("meterInFill").style.width = `${inPct}%`;
            document.getElementById("meterOutFill").style.width = `${outPct}%`;
            document.getElementById("meterDriveFill").style.width = `${Math.min(100, drv)}%`;

            document.getElementById("meterInDb").textContent = inDb <= -60 ? "-∞ dB" : `${inDb.toFixed(1)} dB`;
            document.getElementById("meterOutDb").textContent = outDb <= -60 ? "-∞ dB" : `${outDb.toFixed(1)} dB`;
            document.getElementById("meterDriveDb").textContent = `${drv.toFixed(1)} %`;

            const w = cvScope.width;
            const h = cvScope.height;
            ctx.clearRect(0, 0, w, h);

            ctx.strokeStyle = state.skin === "ataripc" ? "#0d3816" : "#18202a";
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2);
            ctx.stroke();

            if (analyserNode && (isPlaying || useMicInput)) {
                analyserNode.getByteFrequencyData(freqData);
                analyserNode.getByteTimeDomainData(timeData);

                const barCount = 64;
                const barW = w / barCount;
                ctx.fillStyle = state.skin === "ataripc" ? "rgba(51, 255, 102, 0.28)" : "rgba(46, 168, 126, 0.32)";
                for (let i = 0; i < barCount; i++) {
                    const v = freqData[i * 2] / 255.0;
                    const bh = v * (h - 10);
                    ctx.fillRect(i * barW, h - bh, barW - 1, bh);
                }

                ctx.strokeStyle = state.skin === "ataripc" ? "#33ff66" : (state.skin === "flstudio" ? "#ff9100" : "#58f2c2");
                ctx.lineWidth = 2;
                ctx.beginPath();
                for (let i = 0; i < timeData.length; i++) {
                    const x = (i / (timeData.length - 1)) * w;
                    const y = ((timeData[i] - 128) / 128.0) * (h * 0.42) + (h / 2);
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();
            } else {
                ctx.strokeStyle = state.skin === "ataripc" ? "rgba(51,255,102,0.5)" : "rgba(72,213,165,0.45)";
                ctx.lineWidth = 1.6;
                ctx.beginPath();
                const tIdx = Math.max(1, Math.min(5, state.params.type));
                const color = PHOENIX_COLOR_TYPES[tIdx - 1];
                const proc = state.params.process / 100.0;
                for (let px = 0; px < w; px++) {
                    const ph = (px / w) * Math.PI * 4;
                    const raw = Math.sin(ph) * 0.8;
                    const sat = lookupPhxTbl(raw, color.tableOffset);
                    const y = (h / 2) - ((raw * (1 - proc * 0.35) + sat * proc * 0.65) * (h * 0.36));
                    if (px === 0) ctx.moveTo(px, y);
                    else ctx.lineTo(px, y);
                }
                ctx.stroke();
            }

            requestAnimationFrame(tick);
        }
        requestAnimationFrame(tick);
    }

    window.addEventListener("DOMContentLoaded", () => {
        initOriginalSprites();
        bindInteractions();
        bindAudioControls();
        applySkin("cranesong");
        startVisualizerLoop();
    });
})();
