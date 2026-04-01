/**
 *  EffectBreakup.cpp
 *  ONScripter-RU
 *
 *  Emulation of Takashi Toyama's "breakup.dll" NScripter plugin effect.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Components/Window.hpp"
#include "Resources/Support/Resources.hpp"

constexpr int BREAKUP_DIRECTIONS = 8;

// Breakup is divided into a number of "frames".
// Breakup is controlled by the user through breakupFactor which ALWAYS runs between 0 and 1000 (no unit), so changing the following
// values has no effect on the overall "duration" of breakup -- that is user-controlled. What these values do control is the
// relative duration spent in each part of the overall breakup animation. Therefore the concept of "frames" here is very relative.
// You could think of them as "intervals" or "steps" perhaps.

// BREAKUP_DISSOLVE_FRAMES : The number of frames taken for a single tile/circle to go from maximum radius to minimum radius.
constexpr int BREAKUP_DISSOLVE_FRAMES = 1000;
// BREAKUP_WIPE_FRAMES: The number of frames that elapse between the first tile/circle starting to vanish and the last one starting.
// (The time for the diagonal wipe to cross the screen.)
// ex: If it's 0 everything will go flying at once.
// ex: If it's at least DISSOLVE, then some tiles will still be in place after the first one is completely gone.
// ex: If it's many times DISSOLVE, many tiles will still be in place after the first tiles are completely gone.
constexpr int BREAKUP_WIPE_FRAMES = 3000;
// (The total number of frames squished into the 1000 breakupFactor units will be these two added together --
// first we have to reach the last tile [WIPE] to begin its animation, then it has to go through it and complete it [DISSOLVE].)

// BREAKUP_MOVE_FRAMES: The number of frames within the animation of a single tile for which the tile is moving.
// (Should be less than or equal to BREAKUP_DISSOLVE_FRAMES.)
constexpr int BREAKUP_MOVE_FRAMES = 850;

const int breakup_disp_x[BREAKUP_DIRECTIONS]{-7, -7, -5, -4, -2, 1, 3, 5};
const int breakup_disp_y[BREAKUP_DIRECTIONS]{0, 2, 4, 6, 7, 7, 6, 5};

void ONScripter::buildBreakupCellforms() {
	// New method: just load as a file
	if (breakup_cellforms_gpu)
		return;
	breakup_cellforms_gpu = loadGpuImage("breakup-cellforms.png");
}

void ONScripter::buildButterflyCellforms() {
	if (butterfly_cellforms_gpu || butterfly_cellforms_load_attempted)
		return;

	butterfly_cellforms_load_attempted = true;

	// Load butterfly sprite sheet from embedded resources (same approach as breakup-cellforms.png)
	const InternalResource *res = getResource("butterfly-cellforms.png");
	if (res) {
		SDL_RWops *rw        = SDL_RWFromConstMem(res->buffer, static_cast<int>(res->size));
		SDL_Surface *surface = IMG_Load_RW(rw, 0);
		butterfly_cellforms_gpu = gpu.copyImageFromSurface(surface);
		SDL_FreeSurface(surface);
		// The engine uses premultiplied alpha blending; convert from straight alpha
		gpu.multiplyAlpha(butterfly_cellforms_gpu, nullptr);
		butterfly_frame_w = butterfly_cellforms_gpu->w / 4;
		butterfly_frame_h = butterfly_cellforms_gpu->h;
		GPU_SetImageFilter(butterfly_cellforms_gpu, GPU_FILTER_LINEAR);
		GPU_SetBlending(butterfly_cellforms_gpu, true);
		sendToLog(LogLevel::Info, "Loaded butterfly cellforms: %dx%d, frame size %dx%d\n",
		          butterfly_cellforms_gpu->w, butterfly_cellforms_gpu->h, butterfly_frame_w, butterfly_frame_h);
	} else {
		sendToLog(LogLevel::Warn, "butterfly-cellforms.png not found in embedded resources, butterfly overlay disabled\n");
	}

	// Load black butterfly sprite sheet
	const InternalResource *resBlack = getResource("butterfly-cellforms-black.png");
	if (resBlack) {
		SDL_RWops *rwBlack        = SDL_RWFromConstMem(resBlack->buffer, static_cast<int>(resBlack->size));
		SDL_Surface *surfaceBlack = IMG_Load_RW(rwBlack, 0);
		butterfly_cellforms_black_gpu = gpu.copyImageFromSurface(surfaceBlack);
		SDL_FreeSurface(surfaceBlack);
		gpu.multiplyAlpha(butterfly_cellforms_black_gpu, nullptr);
		butterfly_black_frame_w = butterfly_cellforms_black_gpu->w / 4;
		butterfly_black_frame_h = butterfly_cellforms_black_gpu->h;
		GPU_SetImageFilter(butterfly_cellforms_black_gpu, GPU_FILTER_LINEAR);
		GPU_SetBlending(butterfly_cellforms_black_gpu, true);
		sendToLog(LogLevel::Info, "Loaded butterfly cellforms black: %dx%d, frame size %dx%d\n",
		          butterfly_cellforms_black_gpu->w, butterfly_cellforms_black_gpu->h, butterfly_black_frame_w, butterfly_black_frame_h);
	} else {
		sendToLog(LogLevel::Warn, "butterfly-cellforms-black.png not found in embedded resources\n");
	}

	// Generate golden orb texture procedurally (radial gradient, warm gold)
	if (!butterfly_orb_gpu) {
		constexpr int orbSize = 48;
		SDL_Surface *orbSurf  = SDL_CreateRGBSurface(0, orbSize, orbSize, 32,
		                                              0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
		if (orbSurf) {
			float center = orbSize / 2.0f;
			for (int y = 0; y < orbSize; y++) {
				for (int x = 0; x < orbSize; x++) {
					float dx   = x - center + 0.5f;
					float dy   = y - center + 0.5f;
					float dist = std::sqrt(dx * dx + dy * dy) / center;
					if (dist > 1.0f) dist = 1.0f;
					// Soft radial falloff: bright core, gentle fade
					float intensity = std::max(0.0f, 1.0f - dist * dist);
					intensity *= intensity; // sharpen the core for a more spherical look
					// Warm golden color: core is white-gold, edge is deep amber
					float coreFactor = std::max(0.0f, 1.0f - dist * 1.5f);
					coreFactor = std::max(0.0f, coreFactor);
					uint8_t r = static_cast<uint8_t>(std::min(255.0f, (220 + 35 * coreFactor) * intensity));
					uint8_t g = static_cast<uint8_t>(std::min(255.0f, (170 + 60 * coreFactor) * intensity));
					uint8_t b = static_cast<uint8_t>(std::min(255.0f, (50 + 80 * coreFactor) * intensity));
					uint8_t a = static_cast<uint8_t>(255 * intensity);
					auto *pixel = reinterpret_cast<uint32_t *>(
					    static_cast<uint8_t *>(orbSurf->pixels) + y * orbSurf->pitch + x * 4);
					*pixel = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
					         (static_cast<uint32_t>(g) << 8) | r;
				}
			}
			butterfly_orb_gpu = gpu.copyImageFromSurface(orbSurf);
			SDL_FreeSurface(orbSurf);
			gpu.multiplyAlpha(butterfly_orb_gpu, nullptr);
			GPU_SetImageFilter(butterfly_orb_gpu, GPU_FILTER_LINEAR);
			GPU_SetBlending(butterfly_orb_gpu, true);
			sendToLog(LogLevel::Info, "Generated butterfly golden orb texture: %dx%d\n", orbSize, orbSize);
		}
	}
}

void ONScripter::buildProceduralButterflies() {
	if (butterfly_procedural_gpu || butterfly_procedural_built)
		return;
	butterfly_procedural_built = true;

	// Generate a 4-frame butterfly spritesheet procedurally.
	// Each frame shows a golden butterfly with wings at a different angle
	// to create a flapping animation cycle.
	// Frame layout: [wings up | wings mid-up | wings flat | wings mid-down]
	constexpr int frameW = 64;
	constexpr int frameH = 64;
	constexpr int numFrames = 4;
	constexpr int totalW = frameW * numFrames;
	constexpr float cx = frameW / 2.0f;
	constexpr float cy = frameH / 2.0f;

	// Wing flap angles (vertical spread): higher = more open
	// The cycle: up(70°) → mid(45°) → flat(15°) → mid(45°) back
	const float wingAngles[4] = {70.0f, 45.0f, 15.0f, 45.0f};

	SDL_Surface *surf = SDL_CreateRGBSurface(0, totalW, frameH, 32,
	                                          0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	if (!surf) {
		sendToLog(LogLevel::Error, "Failed to create procedural butterfly surface\n");
		return;
	}

	// Clear to transparent
	SDL_FillRect(surf, nullptr, 0);

	for (int frame = 0; frame < numFrames; ++frame) {
		int offsetX = frame * frameW;
		float wingAngleRad = wingAngles[frame] * static_cast<float>(M_PI) / 180.0f;

		// Wing vertical compression based on flap angle
		// When wings are "up" (70°), they spread wide vertically
		// When "flat" (15°), they compress vertically
		float wingSpreadY = std::sin(wingAngleRad);
		float wingSpreadX = 1.0f; // horizontal spread stays constant

		for (int py = 0; py < frameH; ++py) {
			for (int px = 0; px < frameW; ++px) {
				float x = (px - cx) / cx; // -1 to 1
				float y = (py - cy) / cy; // -1 to 1

				float alpha = 0.0f;

				// === BODY ===
				// Elongated vertical ellipse for the body
				float bodyX = x / 0.12f;
				float bodyY = y / 0.45f;
				float bodyDist = bodyX * bodyX + bodyY * bodyY;
				if (bodyDist < 1.0f) {
					float bodyEdge = 1.0f - bodyDist;
					alpha = std::min(1.0f, bodyEdge * 3.0f);
				}

				// === WINGS ===
				// Each wing is an elliptical shape, vertically compressed by wingSpreadY
				float absX = std::fabs(x);
				if (absX > 0.08f) { // outside body
					// Upper wings (larger)
					float uwX = (absX - 0.35f) / 0.35f;
					float uwY = (y - 0.05f) / (0.55f * wingSpreadY + 0.05f);
					// Tilt the upper wings slightly outward and up
					float uwRot = uwX * 0.3f;
					float uwYr = uwY + uwRot;
					float uwDist = uwX * uwX + uwYr * uwYr;
					if (uwDist < 1.0f && y < 0.5f) {
						float wingEdge = 1.0f - uwDist;
						// Scalloped outer edge pattern
						float edgeAngle = std::atan2(uwYr, uwX);
						float scallop = 0.92f + 0.08f * std::sin(edgeAngle * 5.0f);
						if (uwDist < scallop * scallop) {
							float wingAlpha = std::min(1.0f, wingEdge * 2.5f);
							alpha = std::max(alpha, wingAlpha);
						}
					}

					// Lower wings (smaller, rounder)
					float lwX = (absX - 0.28f) / 0.28f;
					float lwY = (y + 0.15f) / (0.38f * wingSpreadY + 0.05f);
					float lwDist = lwX * lwX + lwY * lwY;
					if (lwDist < 1.0f && y > -0.35f) {
						float wingEdge = 1.0f - lwDist;
						float edgeAngle = std::atan2(lwY, lwX);
						float scallop = 0.9f + 0.1f * std::sin(edgeAngle * 4.0f);
						if (lwDist < scallop * scallop) {
							float wingAlpha = std::min(1.0f, wingEdge * 2.5f);
							alpha = std::max(alpha, wingAlpha);
						}
					}
				}

				if (alpha < 0.01f) continue;

				// === COLORING ===
				// Golden color with internal patterns
				float absXn = std::fabs(x);
				float distFromCenter = std::sqrt(x * x + y * y);

				// Wing veins: darker lines radiating from body
				float veinAngle = std::atan2(y, absXn);
				float veinPattern = std::fabs(std::sin(veinAngle * 6.0f));
				float veinMask = (absXn > 0.1f) ? std::max(0.0f, 1.0f - veinPattern * 0.3f * absXn * 2.0f) : 1.0f;

				// Eye spots on upper wings
				float eyeSpotX = absXn - 0.35f;
				float eyeSpotY = y + 0.1f;
				float eyeSpotDist = std::sqrt(eyeSpotX * eyeSpotX + eyeSpotY * eyeSpotY);
				float eyeSpotDarken = (eyeSpotDist < 0.1f) ? 0.6f + 0.4f * (eyeSpotDist / 0.1f) : 1.0f;

				// Gradient: brighter near body, amber at edges
				float edgeFactor = std::min(1.0f, distFromCenter * 1.2f);

				// Core gold: R=255, G=200, B=50 → Edge amber: R=220, G=150, B=20
				float r = (255.0f - 35.0f * edgeFactor) * veinMask * eyeSpotDarken;
				float g = (200.0f - 50.0f * edgeFactor) * veinMask * eyeSpotDarken;
				float b = (50.0f - 30.0f * edgeFactor) * veinMask * eyeSpotDarken;

				// Border darkening for wing edges
				// (alpha is already soft at edges from the ellipse calculation)

				uint8_t finalR = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, r)));
				uint8_t finalG = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, g)));
				uint8_t finalB = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, b)));
				uint8_t finalA = static_cast<uint8_t>(std::min(255.0f, 255.0f * alpha));

				auto *pixel = reinterpret_cast<uint32_t *>(
				    static_cast<uint8_t *>(surf->pixels) + py * surf->pitch + (offsetX + px) * 4);
				*pixel = (static_cast<uint32_t>(finalA) << 24) | (static_cast<uint32_t>(finalB) << 16) |
				         (static_cast<uint32_t>(finalG) << 8) | finalR;
			}
		}

		// === ANTENNAE ===
		// Draw thin curved antennae at the top of each frame
		for (int a = 0; a < 2; ++a) {
			float sign = (a == 0) ? -1.0f : 1.0f;
			for (float t = 0.0f; t <= 1.0f; t += 0.005f) {
				// Curve from head top outward
				float ax = sign * (t * 0.15f + t * t * 0.1f);
				float ay = -0.4f - t * 0.25f + t * t * 0.08f;
				int apx = static_cast<int>((ax + 1.0f) * cx) + offsetX;
				int apy = static_cast<int>((ay + 1.0f) * cy);
				if (apx >= offsetX && apx < offsetX + frameW && apy >= 0 && apy < frameH) {
					// Gold antenna with slight thickness
					for (int dy = -1; dy <= 0; ++dy) {
						int finalPy = apy + dy;
						if (finalPy < 0 || finalPy >= frameH) continue;
						auto *pixel = reinterpret_cast<uint32_t *>(
						    static_cast<uint8_t *>(surf->pixels) + finalPy * surf->pitch + apx * 4);
						uint8_t aAlpha = (dy == 0) ? 255 : 128;
						*pixel = (static_cast<uint32_t>(aAlpha) << 24) | (static_cast<uint32_t>(30) << 16) |
						         (static_cast<uint32_t>(180) << 8) | 240;
					}
				}
			}
		}
	}

	butterfly_procedural_gpu = gpu.copyImageFromSurface(surf);
	SDL_FreeSurface(surf);
	gpu.multiplyAlpha(butterfly_procedural_gpu, nullptr);
	butterfly_procedural_frame_w = frameW;
	butterfly_procedural_frame_h = frameH;
	GPU_SetImageFilter(butterfly_procedural_gpu, GPU_FILTER_LINEAR);
	GPU_SetBlending(butterfly_procedural_gpu, true);
	sendToLog(LogLevel::Info, "Generated procedural butterfly cellforms: %dx%d, frame size %dx%d\n",
	          totalW, frameH, frameW, frameH);
}

bool ONScripter::breakupInitRequired(BreakupID id) {
	return breakupData.count(id) == 0;
}

void ONScripter::initBreakup(BreakupID id, GPU_Image *src, GPU_Rect *src_rect) {
	// sendToLog(LogLevel::Info,"breakup called with breakup factor %u and canvas_w/h %u %u\n", breakupFactor, ons.canvas_width, ons.canvas_height);
	int cellFactor = ons.new_breakup_implementation ? BREAKUP_CELLSEPARATION : BREAKUP_CELLWIDTH;
	int w{0}, h{0};
	if ((id.type == BreakupType::SPRITE_TIGHTFIT || id.type == BreakupType::BUTTERFLY_SPRITE_TIGHTFIT) && ons.new_breakup_implementation) {
		if (src_rect) {
			w = src_rect->w;
			h = src_rect->h;
		} else {
			w = src->w;
			h = src->h;
		}
	} else {
		w = window.canvas_width;
		h = window.canvas_height;
	}

	int numCellsX = ((w + cellFactor - 1) / cellFactor) + 1;
	int numCellsY = ((h + cellFactor - 1) / cellFactor) + 1;

	BreakupData &data = breakupData[id];
	data.breakup_cells.resize(numCellsX * numCellsY);
	data.diagonals.resize(numCellsX + numCellsY - 1);
	data.wInCellsFloat = (static_cast<float>(w) / (1.0f * cellFactor));
	data.hInCellsFloat = (static_cast<float>(h) / (1.0f * cellFactor));
	data.cellFactor    = cellFactor;
	data.numCellsX     = numCellsX;
	data.numCellsY     = numCellsY;

	if (!ons.new_breakup_implementation) {
		buildBreakupCellforms();
		if (!breakup_cellform_index_grid) {
			breakup_cellform_index_grid    = gpu.createImage(numCellsX, numCellsY, 4);
			breakup_cellform_index_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, numCellsX, numCellsY, 32, 0, 0, 0, 0);
		}
	}

	// For butterfly breakup types, determine which cells have visible source content
	// so we only render butterflies over non-transparent regions.
	// The grid is indexed spatially as [cell_y * numCellsX + cell_x] because
	// the breakup_cells array is stored in diagonal order, not row-major.
	bool isButterfly = (id.type == BreakupType::BUTTERFLY_SPRITE_CANVAS ||
	                    id.type == BreakupType::BUTTERFLY_SPRITE_TIGHTFIT ||
	                    id.type == BreakupType::BUTTERFLY_SPRITESET ||
	                    id.type == BreakupType::BUTTERFLY_GLOBAL);
	if (isButterfly && src) {
		data.cellHasContent.resize(numCellsX * numCellsY, false);
		data.prevBreakupFactor = -1;
		SDL_Surface *surf = GPU_CopySurfaceFromImage(src);
		if (surf) {
			int srcOffX = src_rect ? static_cast<int>(src_rect->x) : 0;
			int srcOffY = src_rect ? static_cast<int>(src_rect->y) : 0;
			int bpp     = surf->format->BytesPerPixel;
			// Iterate over spatial grid positions (cx, cy)
			for (int cy = 0; cy < numCellsY; ++cy) {
				for (int cx = 0; cx < numCellsX; ++cx) {
					int basePx = cx * cellFactor + srcOffX;
					int basePy = cy * cellFactor + srcOffY;
					bool found = false;
					// Check every pixel in the cell for non-transparent content
					for (int py = basePy; py < basePy + cellFactor && !found; ++py) {
						for (int px = basePx; px < basePx + cellFactor && !found; ++px) {
							if (px >= 0 && px < surf->w && py >= 0 && py < surf->h && bpp == 4) {
								auto *pixel = static_cast<uint8_t *>(surf->pixels) + py * surf->pitch + px * bpp;
								uint32_t pval;
								std::memcpy(&pval, pixel, 4);
								uint8_t r, g, b, a;
								SDL_GetRGBA(pval, surf->format, &r, &g, &b, &a);
								if (a > 0) found = true;
							}
						}
					}
					data.cellHasContent[cy * numCellsX + cx] = found;
				}
			}

			// Create gold mask: same dimensions as source, all opaque pixels → solid gold.
			// This is drawn using the unbroken-region blitter for a smooth seamless silhouette.
			SDL_Surface *goldSurf = SDL_CreateRGBSurface(0, surf->w, surf->h,
			                                             32, surf->format->Rmask, surf->format->Gmask,
			                                             surf->format->Bmask, surf->format->Amask);
			if (goldSurf) {
				// Gold color: warm amber (premultiplied)
				constexpr uint8_t goldR = 230, goldG = 190, goldB = 60;
				SDL_LockSurface(goldSurf);
				for (int py = 0; py < surf->h; ++py) {
					for (int px = 0; px < surf->w; ++px) {
						auto *srcPixel = static_cast<uint8_t *>(surf->pixels) + py * surf->pitch + px * bpp;
						auto *dstPixel = static_cast<uint8_t *>(goldSurf->pixels) + py * goldSurf->pitch + px * 4;
						uint32_t srcVal;
						std::memcpy(&srcVal, srcPixel, 4);
						uint8_t r, g, b, a;
						SDL_GetRGBA(srcVal, surf->format, &r, &g, &b, &a);
						// Premultiplied gold with source alpha
						uint8_t gR = static_cast<uint8_t>(goldR * a / 255);
						uint8_t gG = static_cast<uint8_t>(goldG * a / 255);
						uint8_t gB = static_cast<uint8_t>(goldB * a / 255);
						uint32_t goldVal = SDL_MapRGBA(goldSurf->format, gR, gG, gB, a);
						std::memcpy(dstPixel, &goldVal, 4);
					}
				}
				SDL_UnlockSurface(goldSurf);
				data.goldMaskGpu = gpu.copyImageFromSurface(goldSurf);
				if (data.goldMaskGpu) {
					GPU_SetImageFilter(data.goldMaskGpu, GPU_FILTER_LINEAR);
					GPU_SetBlending(data.goldMaskGpu, true);
				}
				SDL_FreeSurface(goldSurf);
			}
			SDL_FreeSurface(surf);
		}
	}
}

void ONScripter::oncePerBreakupEffectBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX,
                                                  int numCellsY) {
	if (!ons.new_breakup_implementation)
		return;
	BreakupData &data = breakupData[id];
	if (data.breakup_mode.has() && data.breakup_mode.get() == breakupDirectionFlagset) {
		// nothing to do, we're all set up
		return;
	}

	std::srand(id.hash);

	data.breakup_mode.set(breakupDirectionFlagset);
	data.n_cells    = numCellsX * numCellsY;
	data.tot_frames = BREAKUP_DISSOLVE_FRAMES + BREAKUP_WIPE_FRAMES;

	int totalDiagCount      = numCellsX + numCellsY - 1;
	int n                   = 0;
	BreakupCell *cells      = breakupData[id].breakup_cells.data();
	BreakupCell **diagonals = breakupData[id].diagonals.data();
	for (int thisDiagNo = 0; thisDiagNo < totalDiagCount; thisDiagNo++) {
		diagonals[thisDiagNo] = &cells[n];
		for (int x = thisDiagNo, y = 0; (x >= 0) && (y < numCellsY); x--, y++) {
			if (x >= numCellsX)
				continue; // until it gets back into range -- this removes the need for two loops
			// calculate initial state
			int state = BREAKUP_DISSOLVE_FRAMES;
			if (totalDiagCount > 1) { // prevent divide by zero
				// TODO: this gives uneven distribution, 20 should most likely depend on thisDiagNo/totalDiagCount.
				int fakeDiagNo = thisDiagNo - (std::rand() % 20);
				if (fakeDiagNo < 0)
					fakeDiagNo = 0;
				state += (fakeDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
			}
			// First cells iterated have state of only BREAKUP_DISSOLVE_FRAMES, so they will be first to disappear or last to appear.
			// If breakup mode is LEFT, then they should be at x=0.
			// If breakup mode is LOWER, then they should be at y=maxCell-1, because top-left is 0,0 for textures.
			cells[n].cell_x   = (breakupDirectionFlagset & BREAKUP_MODE_LEFT) ? x : numCellsX - x - 1;
			cells[n].cell_y   = (breakupDirectionFlagset & BREAKUP_MODE_LOWER) ? numCellsY - y - 1 : y;
			cells[n].diagonal = thisDiagNo;
			cells[n].state    = state;

			int x_dir = 1;
			int y_dir = -1; // flip the y-axis here so that we can express below angles between 0~90 for the top right quadrant, as is normal in math
			if (breakupDirectionFlagset & BREAKUP_MODE_JUMBLE) {
				x_dir = -x_dir;
				y_dir = -y_dir;
			}
			if (breakupDirectionFlagset & BREAKUP_MODE_LEFT) {
				x_dir = -x_dir;
			}
			if (breakupDirectionFlagset & BREAKUP_MODE_LOWER) {
				y_dir = -y_dir;
			}

			int ax                    = (thisDiagNo - (numCellsY - 1));
			ax                        = ax > 0 ? x - ax : x;
			int ay                    = (thisDiagNo - (numCellsX - 1));
			ay                        = ay > 0 ? y - ay : y;
			double angle              = ax == 0 ? M_PI / 2.0 : std::atan2(ay, ax);
			int plusminus50           = (std::rand() % 101) - 50;
			double plusminus45degrees = M_PI / 4.0 * plusminus50 / 50.0;
			angle += plusminus45degrees;

			cells[n].xMovement = x_dir * std::cos(angle);
			cells[n].yMovement = y_dir * std::sin(angle);

			++n;
		}
	}
}

void ONScripter::deinitBreakup(BreakupID id) {
	if (breakupInitRequired(id)) {
		return;
	}
	auto it = breakupData.find(id);
	if (it != breakupData.end()) {
		if (it->second.goldMaskGpu) {
			GPU_FreeImage(it->second.goldMaskGpu);
			it->second.goldMaskGpu = nullptr;
		}
		breakupData.erase(it);
	}
}

void ONScripter::effectBreakupNew(BreakupID id, int breakupFactor) {
	BreakupData &data    = breakupData[id];
	BreakupCell *myCells = data.breakup_cells.data();

	int duration = 1000;
	int frame    = data.tot_frames * breakupFactor / duration;

	int maximumDiagonal = 0;
	int state           = 0;
	bool touched        = false;

	for (int n = 0; n < data.n_cells; ++n) {
		BreakupCell &cell = myCells[n];
		state             = cell.state - frame;
		cell.disp_x       = 0;
		cell.disp_y       = 0;
		touched           = false;
		cell.resizeFactor = 1.0f; // If we haven't started the animation yet
		if (state < BREAKUP_DISSOLVE_FRAMES) {
			// We started the animation, so now the radius should reduce to zero according to state
			cell.resizeFactor = state <= 0 ? 0.0f : 1.0f * state / BREAKUP_DISSOLVE_FRAMES;
			touched           = true;
		}
		if (state < BREAKUP_MOVE_FRAMES && state > 0) {
			// If we've started moving the position
			cell.disp_x = cell.xMovement * (BREAKUP_MOVE_FRAMES - state);
			cell.disp_y = cell.yMovement * (BREAKUP_MOVE_FRAMES - state);
			touched     = true;
		}
		if (touched && cell.diagonal > maximumDiagonal) {
			maximumDiagonal = cell.diagonal;
		}
	}
	data.maxDiagonalToContainBrokenCells = maximumDiagonal;
}

void ONScripter::oncePerFrameBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX, int numCellsY) {
	auto &data = breakupData[id];

	if (ons.new_breakup_implementation) {
		std::srand(static_cast<unsigned int>(id.hash));
	}

	data.breakup_mode.set(breakupDirectionFlagset);

	int totalDiagCount = numCellsX + numCellsY - 1;
	data.n_cells       = numCellsX * numCellsY;
	data.tot_frames    = BREAKUP_DISSOLVE_FRAMES + BREAKUP_WIPE_FRAMES;
	data.prev_frame    = 0;

	int n = 0, dir = 1;
	auto myCells = data.breakup_cells.data();
	for (int thisDiagNo = 0; thisDiagNo < totalDiagCount; thisDiagNo++) {
		int state = BREAKUP_DISSOLVE_FRAMES;
		if (!ons.new_breakup_implementation && totalDiagCount > 1) // prevent divide by zero
			state += (thisDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
		for (int x = thisDiagNo, y = 0; (x >= 0) && (y < numCellsY); x--, y++) {
			if (x >= numCellsX)
				continue; // until it gets back into range -- this removes the need for two loops

			if (ons.new_breakup_implementation) {
				state = BREAKUP_DISSOLVE_FRAMES;
				if (totalDiagCount > 1) { // prevent divide by zero
					int fakeDiagNo = thisDiagNo - (std::rand() % 20);
					if (fakeDiagNo < 0)
						fakeDiagNo = 0;
					state += (fakeDiagNo * BREAKUP_WIPE_FRAMES / (totalDiagCount - 1));
				}
			}

			myCells[n].cell_x = x;
			myCells[n].cell_y = y;
			if (!(breakupDirectionFlagset & BREAKUP_MODE_LEFT))
				myCells[n].cell_x = numCellsX - x - 1;
			if (breakupDirectionFlagset & BREAKUP_MODE_LOWER)
				myCells[n].cell_y = numCellsY - y - 1;
			myCells[n].dir    = dir;
			myCells[n].state  = state;
			myCells[n].radius = 0;
			dir               = (dir + 1) & (BREAKUP_DIRECTIONS - 1);
			++n;
		}
	}
}

void ONScripter::effectBreakupOld(BreakupID id, int breakupFactor) {
	auto &data   = breakupData[id];
	auto myCells = data.breakup_cells.data();
	int duration = 1000;

	int x_dir = -1;
	int y_dir = -1;

	int frame      = data.tot_frames * breakupFactor / duration;
	int frame_diff = frame - data.prev_frame;
	if (frame_diff == 0)
		return;

	data.prev_frame += frame_diff;
	frame_diff = -frame_diff;

	int breakupDirectionFlagset = data.breakup_mode.get();
	if (breakupDirectionFlagset & BREAKUP_MODE_JUMBLE) {
		x_dir = -x_dir;
		y_dir = -y_dir;
	}
	if (!(breakupDirectionFlagset & BREAKUP_MODE_LEFT)) {
		x_dir = -x_dir;
	}
	if (breakupDirectionFlagset & BREAKUP_MODE_LOWER) {
		y_dir = -y_dir;
	}

	int state{0};
	for (int n = 0; n < data.n_cells; ++n) {
		myCells[n].state += frame_diff;
		state             = myCells[n].state;
		myCells[n].disp_x = 0;
		myCells[n].disp_y = 0;
		myCells[n].radius = 0;
		// If we haven't started the animation yet
		myCells[n].radius = BREAKUP_CELLFORMS; // greater than the maximum index, indicating "do not apply mask"
		if (state < BREAKUP_DISSOLVE_FRAMES) {
			// We started the animation, so now the radius should reduce to zero according to state
			myCells[n].radius = state <= 0 ? 0 : BREAKUP_CELLFORMS * state / BREAKUP_DISSOLVE_FRAMES;
		}
		if (state < BREAKUP_MOVE_FRAMES && state > 0) {
			// If we've started moving the position
			myCells[n].disp_x = x_dir * breakup_disp_x[myCells[n].dir] * (state - BREAKUP_MOVE_FRAMES) / 10;
			myCells[n].disp_y = y_dir * breakup_disp_y[myCells[n].dir] * (BREAKUP_MOVE_FRAMES - state) / 10;
			// ehhhhhhhh don't really like this method of calculation... the divisor is pretty arbitrary
		}
		int c = (myCells[n].radius * 255) / BREAKUP_CELLFORMS;
		setSurfacePixel(breakup_cellform_index_surface, myCells[n].cell_x, myCells[n].cell_y,
		                SDL_MapRGBA(breakup_cellform_index_surface->format, c, c, c, 255));
	}
	GPU_GetTarget(breakup_cellform_index_grid);
	gpu.updateImage(breakup_cellform_index_grid, nullptr, breakup_cellform_index_surface, nullptr, false);
}
