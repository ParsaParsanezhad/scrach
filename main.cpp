#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfx.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
using namespace std;

const int WINDOW_WIDTH  = 1000, WINDOW_HEIGHT = 600;
const int MENU_H = 48;

// ناحیه‌ها
SDL_Rect leftPanel  = { 80, 50, 200, 550 };
SDL_Rect workspace  = { 280, 50, 470, 550 };
SDL_Rect stagePanel = { 710, 50, 290, 550 };

int spriteX = 845, spriteY = 315, spriteR = 20;

void drawRect(SDL_Renderer* ren, SDL_Rect r, SDL_Color fill) {
    SDL_SetRenderDrawColor(ren, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(ren, &r);
}

void drawCircle(SDL_Renderer* ren, int cx, int cy, int r, SDL_Color color) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x*x + y*y <= r*r) {
                SDL_RenderDrawPoint(ren, cx + x, cy + y);
            }
        }
    }
}

bool pointInRect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

struct BlockType {
    SDL_Color color;
    SDL_Rect  protoRect;
    string name;
};

struct BlockInstance {
    int typeIndex;
    SDL_Rect rect;
    int lastValidX, lastValidY;
    bool dragging = false;
    int dragStartX = 0, dragStartY = 0;
};

void writeCenteredText(SDL_Renderer *ren, TTF_Font* font, string text, SDL_Rect rect){
    SDL_Color white={255, 255, 255, 255};
    SDL_Surface *surf=TTF_RenderText_Blended(font, text.c_str(), white);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    SDL_Rect dst = { rect.x + (rect.w - tw)/2, rect.y + (rect.h - th)/2, tw, th };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void writeLeftText(SDL_Renderer *ren, TTF_Font* font, string text, SDL_Rect rect, int pad = 8){
    SDL_Color white={255, 255, 255, 255};
    SDL_Surface *surf=TTF_RenderText_Blended(font, text.c_str(), white);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    SDL_Rect dst = { rect.x + pad, rect.y + (rect.h - th)/2, tw, th };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

string getProjectPath() {
    char* base = SDL_GetBasePath();
    string path;
    if (base) {
        path = string(base) + "project.txt";
        SDL_free(base);
    } else {
        path = "project.txt";
    }
    return path;
}

bool saveBlocks(const string& path, const vector<BlockInstance>& instances) {
    ofstream out(path);
    if (!out) return false;
    out << instances.size() << "\n";
    for (auto& b : instances) {
        out << b.typeIndex << " "
            << b.rect.x << " " << b.rect.y << " "
            << b.rect.w << " " << b.rect.h << "\n";
    }
    return true;
}

bool loadBlocks(const string& path, vector<BlockInstance>& instances) {
    ifstream in(path);
    if (!in) return false;
    size_t n = 0;
    in >> n;
    instances.clear();
    for (size_t i = 0; i < n; ++i) {
        BlockInstance b;
        in >> b.typeIndex
           >> b.rect.x >> b.rect.y
           >> b.rect.w >> b.rect.h;
        b.lastValidX = b.rect.x;
        b.lastValidY = b.rect.y;
        b.dragging = false;
        instances.push_back(b);
    }
    return true;
}

// دیالوگ پرسش برای ذخیره قبل از New
int confirmSaveBeforeNew(SDL_Window* win) {
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes (Save)" },
        { 0, 2, "No (Don't Save)" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" }
    };

    const SDL_MessageBoxColorScheme colorScheme = {
        {
            { 255, 255, 255 }, // background
            {   0,   0,   0 }, // text
            { 100, 100, 100 }, // button border
            { 220, 220, 220 }, // button background
            {   0,   0,   0 }  // button selected
        }
    };

    SDL_MessageBoxData data = {};
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.window = win;
    data.title = "New Project";
    data.message = "Do you want to save the current project before creating a new one?";
    data.numbuttons = 3;
    data.buttons = buttons;
    data.colorScheme = &colorScheme;

    int buttonid = 0;
    SDL_ShowMessageBox(&data, &buttonid);
    return buttonid; // 1=Yes, 2=No, 0=Cancel
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* win = SDL_CreateWindow(
        "main window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* font = TTF_OpenFont("calibrib.ttf", 18);

    vector <BlockType> blockTypes = {
        { {  70,150,255,255}, { 0,  50, 80, 50 } , "motion"},
        { { 150,100,255,255}, { 0, 100, 80, 50 } , "looks"},
        { { 210, 100,210,255}, { 0, 150, 80, 50 }, "sound" },
        { { 255,210,  0,255}, { 0, 200, 80, 50 } , "events"},
        { { 255,170, 25,255}, { 0, 250, 80, 50 } , "control"},
        { {  90,180,210,255}, { 0, 300, 80, 50 } , "senses"},
        { {  90,190, 90,255}, { 0, 350, 80, 50 } , "operators"},
        { { 255,140, 25,255}, { 0, 400, 80, 50 } , "variables"}
    };

    vector <BlockInstance> instances;

    int logo_w, logo_h;
    SDL_Texture *logo= IMG_LoadTexture(ren, "logo.png");
    SDL_QueryTexture(logo, NULL, NULL, &logo_w, &logo_h);
    SDL_Rect logoSize ={-20, -35, 120, 120};

    SDL_Rect fileBtn = {10, 8, 60, 30};
    SDL_Rect fileItemNew  = {10, MENU_H, 120, 28};
    SDL_Rect fileItemSave = {10, MENU_H + 28, 120, 28};
    SDL_Rect fileItemLoad = {10, MENU_H + 56, 120, 28};
    bool fileMenuOpen = false;

    string projectPath = getProjectPath();
    cout << "Project file path: " << projectPath << endl;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x, my = e.button.y;
                bool consumed = false;

                if (pointInRect(mx, my, fileBtn)) {
                    fileMenuOpen = !fileMenuOpen;
                    cout << "File menu toggled: " << fileMenuOpen << endl;
                    consumed = true;
                } else if (fileMenuOpen) {
                    if (pointInRect(mx, my, fileItemNew)) {
                        // پرسش برای ذخیره قبل از پاک کردن
                        if (!instances.empty()) {
                            int res = confirmSaveBeforeNew(win);
                            if (res == 1) { // Yes
                                bool ok = saveBlocks(projectPath, instances);
                                SDL_ShowSimpleMessageBox(ok ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_ERROR,
                                                         "Save", ok ? "Saved successfully." : "Save failed!",
                                                         win);
                                if (ok) instances.clear();
                            } else if (res == 2) { // No
                                instances.clear();
                            } else {
                                // Cancel -> هیچ کاری نکن
                            }
                        } else {
                            // اگر چیزی نیست، مستقیم پاک کن
                            instances.clear();
                        }

                        fileMenuOpen = false;
                        cout << "New clicked" << endl;
                        consumed = true;

                    } else if (pointInRect(mx, my, fileItemSave)) {
                        bool ok = saveBlocks(projectPath, instances);
                        cout << "Save clicked: " << (ok ? "OK" : "FAIL") << endl;
                        SDL_ShowSimpleMessageBox(ok ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_ERROR,
                                                 "Save", ok ? "Saved successfully." : "Save failed!",
                                                 win);
                        fileMenuOpen = false;
                        consumed = true;
                    } else if (pointInRect(mx, my, fileItemLoad)) {
                        bool ok = loadBlocks(projectPath, instances);
                        cout << "Load clicked: " << (ok ? "OK" : "FAIL") << endl;
                        SDL_ShowSimpleMessageBox(ok ? SDL_MESSAGEBOX_INFORMATION : SDL_MESSAGEBOX_ERROR,
                                                 "Load", ok ? "Loaded successfully." : "Load failed!",
                                                 win);
                        fileMenuOpen = false;
                        consumed = true;
                    } else {
                        fileMenuOpen = false;
                    }
                }

                if (my < MENU_H) consumed = true;
                if (consumed) continue;

                bool picked = false;
                for (int i = (int)instances.size() - 1; i >= 0; --i) {
                    if (pointInRect(mx, my, instances[i].rect)) {
                        instances[i].dragging = true;
                        instances[i].dragStartX = mx - instances[i].rect.x;
                        instances[i].dragStartY = my - instances[i].rect.y;
                        BlockInstance temp = instances[i];
                        instances.erase(instances.begin() + i);
                        instances.push_back(temp);
                        picked = true;
                        break;
                    }
                }

                if (!picked) {
                    for (int i = 0; i < (int)blockTypes.size(); ++i) {
                        if (pointInRect(mx, my, blockTypes[i].protoRect)) {
                            BlockInstance b;
                            b.typeIndex = i;
                            b.rect = blockTypes[i].protoRect;
                            b.dragging = true;
                            b.dragStartX = mx - b.rect.x;
                            b.dragStartY = my - b.rect.y;
                            b.lastValidX = b.rect.x;
                            b.lastValidY = b.rect.y;
                            instances.push_back(b);
                            break;
                        }
                    }
                }
            }

            if (e.type == SDL_MOUSEMOTION) {
                int mx = e.motion.x, my = e.motion.y;
                if (!instances.empty()) {
                    BlockInstance& b = instances.back();
                    if (b.dragging) {
                        b.rect.x = mx - b.dragStartX;
                        b.rect.y = my - b.dragStartY;
                    }
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && !instances.empty()) {
                BlockInstance &b = instances.back();
                if (b.dragging) {
                    b.dragging = false;
                    int cx = b.rect.x + b.rect.w / 2, cy = b.rect.y + b.rect.h / 2;

                    if (pointInRect(cx, cy, workspace)) {
                        b.lastValidX = b.rect.x;
                        b.lastValidY = b.rect.y;
                    } else if (pointInRect(cx, cy, stagePanel)) {
                        b.rect.x = b.lastValidX;
                        b.rect.y = b.lastValidY;
                        if (!pointInRect(b.rect.x + b.rect.w/2, b.rect.y + b.rect.h/2, workspace))
                            instances.pop_back();
                    } else {
                        instances.pop_back();
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
        SDL_RenderClear(ren);

        boxRGBA(ren, 0, 0, WINDOW_WIDTH, MENU_H, 160, 70, 165, 200);
        SDL_RenderCopy(ren, logo, NULL, &logoSize);

        boxRGBA(ren, fileBtn.x, fileBtn.y, fileBtn.x + fileBtn.w, fileBtn.y + fileBtn.h, 120, 50, 130, 255);
        writeLeftText(ren, font, "File", fileBtn, 10);

        drawRect(ren, leftPanel,  {255,255,255,255});
        drawRect(ren, workspace,  {248,249,255,255});
        drawRect(ren, stagePanel, {240,244,255,255});
        vlineRGBA(ren, 280, 50, WINDOW_HEIGHT, 0, 0, 0, 255);
        vlineRGBA(ren, 710, 50, WINDOW_HEIGHT, 0, 0, 0, 255);

        for (auto& bt : blockTypes){
            drawRect(ren, bt.protoRect, bt.color);
            writeCenteredText(ren, font, bt.name, bt.protoRect);
        }

        for (auto& b : instances) {
            drawRect(ren, b.rect, blockTypes[b.typeIndex].color);
            writeCenteredText(ren, font, blockTypes[b.typeIndex].name, b.rect);
        }

        drawCircle(ren, spriteX, spriteY, spriteR, {255,159,28,255});

        if (fileMenuOpen) {
            boxRGBA(ren, fileItemNew.x,  fileItemNew.y,  fileItemNew.x + fileItemNew.w,  fileItemNew.y + fileItemNew.h,  90, 90, 90, 255);
            boxRGBA(ren, fileItemSave.x, fileItemSave.y, fileItemSave.x + fileItemSave.w, fileItemSave.y + fileItemSave.h, 90, 90, 90, 255);
            boxRGBA(ren, fileItemLoad.x, fileItemLoad.y, fileItemLoad.x + fileItemLoad.w, fileItemLoad.y + fileItemLoad.h, 90, 90, 90, 255);

            writeLeftText(ren, font, "New",  fileItemNew, 10);
            writeLeftText(ren, font, "Save", fileItemSave, 10);
            writeLeftText(ren, font, "Load", fileItemLoad, 10);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_DestroyTexture(logo);
    IMG_Quit();
    SDL_Quit();
    return 0;
}