#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>


#define uint unsigned int
#define MAX_ENEMIES 100
#define MAX_PROJECTILES 250
#define MAX_PROPS 100
#define MAX_ITEMS 200
#define WEP_OFFSET 8
#define AMO_OFFSET 3
#define MKT_OFFSET 0
#define BAG_OFFSET 2
#define MHP_OFFSET 1
#define MAP_SIZE 256

static RenderTexture2D canvas;
static RenderTexture2D lightingTexture;
static RenderTexture2D combinedTexture;
//Assets
static Texture2D texEnemies;
static Texture2D texGround;
static Texture2D texLight;
static Texture2D texProps;
static Texture2D texWeapons;
static Texture2D texItems;
static Model mdSkybox;
static Shader lightShader;
static int lightmapULoc;
static Sound revShoot;
static Sound nadeExplosion;
static Sound sgunShoot;
static Sound enemyHit;
static Sound playerHit;
static Sound itemPickUp;
static Music lvl[3];

enum EnemyType {
    ET_Amogus,
    ET_Impostor,

    ET_LAST_ENTRY,
};

enum WeaponType {
    WT_Pistol,
    WT_Launcher,
    WT_Shotgun,

    WT_LAST_ENTRY,
};

enum EnemyState {
    ES_Stand,
    ES_Wander,
    ES_Pursue,
    ES_Attack,
};

typedef struct {
    bool unlocked;
    uint damage;
    uint ammo;
    uint ammoCap;
    uint frames;
    uint curFrame;
    double frameTimer;
    double frameTime;
    Rectangle spriteRect;
    void (*OnShoot)(void);
} Weapon;

typedef struct enemy{
    bool alive;
    int health;
    uint frames;
    int curFrame;
    double frameTimer;
    double frameTime;
    int state;
    uint speed;
    float attackRange;
    float detectRange;
    Vector2 position;
    Vector2 velocity;
    Rectangle spriteRect;
    void (*OnAttack)(void);
    void (*OnDeath)(struct enemy*);
} Enemy;

typedef struct {
    bool active;
    Vector3 position;
    Rectangle spriteRect;
} Prop;

typedef struct {
    bool active;
    Vector3 position;
    Rectangle spriteRect;
    void* data;
    void (*OnPickUp)(void*);
} Item;

typedef struct {
    bool active;
    Vector3 position;
    Vector3 velocity;
    uint speed;
    int damage;
} Projectile;

typedef struct {
    double unpausedTime;
    double totalTime;
    double deltaTime;
    void (*UpdateFunc)(void);
    void (*DrawFunc)(void);
    Music currentMusic;
    bool isPaused;
    bool isUnfocused;
} GameState;

#define MAX(x,y) x > y ? x : y
#define MIN(x,y) x < y ? x : y

void PlaySoundRPitch(Sound sound);
void PlaySoundRPitchDirectional(Sound sound, Vector2 source);
void OnShootLaser(void);
void OnShootLauncher(void);
void OnShootShotgun(void);
void OnAttackAmogus(void);
void OnDeathAmogus(Enemy* e);
void Update(void);
void UpdateView(void);
void UpdateWeapon(void);
void UpdateEnemies(void);
void UpdateEnemy(Enemy* e);
void UpdateWin(void);
void UpdateGameOver(void);
void DrawScene(void);
void DrawEnemies(void);
void DrawWeapon(void);
void DrawSkybox(void);
void DrawUI(void);
void DrawProjectiles(void);
void Draw(void);
void DrawGameOver(void);
void DrawWin(void);
void RenderLightTexture(void);
void LoadAssets(void);
void UnloadAssets(void);
void SpawnEnemy(int type, float x, float y);
void SpawnProp(int id, float x, float y);
void DeleteItems(void);
void SpawnProjectile(float x, float y, Vector3 velocity, int dmg, uint spd);
void SpawnAmmo(int weapon, int amount, float x, float y);
void SpawnWeapon(int weapon, float x, float y);
void SpawnRandomItem(int mod, float x, float y);

static Vector2 playerPos = {0,0};
static Vector2 playerVel = {0,0};
static Vector2 rotation = {0,0};
static uint playerSpeed = 20;
static int playerHealth = 100;
static int playerHealthMax = 100;
static uint selectedWeapon = 0;
static int curMusic = 0;
static Vector2 mouseDelta = {0,0};
static Vector2 mouseSensitivity = {20.0,10.0};
static Weapon Weapons[WT_LAST_ENTRY] = {
    {
        .unlocked = true,
        .damage = 50,
        .ammo = 66,
        .ammoCap = 300,
        .frames = 3,
        .curFrame = 0,
        .frameTimer = 0,
        .frameTime = 0.2,
        .spriteRect = {0, 0, 64, 120},
        .OnShoot = &OnShootLaser,
    },
    {
        .unlocked = false,
        .damage = 150,
        .ammo = 5,
        .ammoCap = 50,
        .frames = 3,
        .curFrame = 0,
        .frameTimer = 0,
        .frameTime = 0.5,
        .spriteRect = {0, 120, 100, 120},
        .OnShoot = &OnShootLauncher,
    },
    {
        .unlocked = false,
        .damage = 10,
        .ammo = 22,
        .ammoCap = 100,
        .frames = 3,
        .curFrame = 0,
        .frameTimer = 0,
        .frameTime = 0.3,
        .spriteRect = {0, 240, 100, 120},
        .OnShoot = &OnShootShotgun,
    },
};

static Prop Props[MAX_PROPS] = {0};
static Enemy Enemies[MAX_ENEMIES] = {0};
static Projectile Projectiles[MAX_PROJECTILES] = {0};
static Item Items[MAX_ITEMS] = {0};


GameState state;
Camera3D cam = {
    .fovy = 90,
    .up = {0,1,0},
    .projection = CAMERA_PERSPECTIVE,
};
int curMaxEnemies = 10;
int curEnemies = 10;
int curWave = 0;
int score = 0;

Ray debugRays[8] = {0};

//get fade out volume
float GetMusicAdaptiveVolume(const Music* m) {
    float l = GetMusicTimeLength(*m);
    float t = GetMusicTimePlayed(*m);
    t = t > l ? l : t;
    float v = l - t;
    if(v < 25.0f) {
        v = 0.3f * v / 25.0f;
    }
    else {
        v = 0.3f;
    }
    return v;
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1280;
    const int screenHeight = 720;
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Sus Shooter WIP");
    InitAudioDevice();
    DisableCursor();
    LoadAssets();
    
    for(int i = 0; i < curMaxEnemies; i++) {
        SpawnEnemy(ET_Amogus, GetRandomValue(-90, 90), GetRandomValue(-90, 90));
    }
    for(int i = 0; i < MAX_PROPS; i++) {
        SpawnProp(GetRandomValue(1, 10), GetRandomValue(-90, 90), GetRandomValue(-90, 90));
    }

    state.DrawFunc = &Draw;
    state.UpdateFunc = &Update;
    curMusic = GetRandomValue(0, 2);
    PlayMusicStream(lvl[curMusic]);
    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        state.UpdateFunc();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        state.DrawFunc();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    DeleteItems();
    StopMusicStream(lvl[curMusic]);
    UnloadAssets();
    CloseAudioDevice();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void AddAmmo(int weapon, int amount) {
    Weapons[weapon].ammo += amount;
    if(Weapons[weapon].ammo > Weapons[weapon].ammoCap) {
        Weapons[weapon].ammo = Weapons[weapon].ammoCap;
    }
}

void HealPlayer(int hp) {
    playerHealth += hp;
    if(playerHealth > playerHealthMax) {
        playerHealth = playerHealthMax;
    }
}

void ChangeAmmoCap(int weapon, int amount) {
    Weapons[weapon].ammoCap += amount;
    if(Weapons[weapon].ammoCap < 10) {
        Weapons[weapon].ammoCap = 10;
    }
    if(Weapons[weapon].ammo > Weapons[weapon].ammoCap) {
        Weapons[weapon].ammo = Weapons[weapon].ammoCap;
    }
}

void ChangePlayerMaxHp(int amount) {
    playerHealthMax += amount;
    if(playerHealthMax < 3) {
        playerHealthMax = 3;
    }
    if(playerHealth > playerHealthMax){
        playerHealth = playerHealthMax;
    }
}

void OnPickUpAmmo(void* data) {
    int* d = (int*)data;
    AddAmmo(d[0], d[1]);
}

void OnPickUpWeapon(void* data) {
    int* d = (int*)data;
    Weapons[d[0]].unlocked = true;
    AddAmmo(d[0], 1);
}

void OnPickUpAmmoBag(void* data) {
    int* d = (int*)data;
    ChangeAmmoCap(d[0], d[1]);
}

void OnPickUpMedkit(void* data) {
    int* d = (int*)data;
    HealPlayer(d[0]);
}

void OnPickUpMaxHP(void* data) {
    int* d = (int*)data;
    ChangePlayerMaxHp(d[0]);
}

void DamageEnemy(Enemy* e, uint dmg) {
    static const Enemy* lastTarget = NULL;
    static double lastCalled = 0;
    if(!e->alive) { return; }
    e->health -= dmg;
    if(lastTarget != e || lastCalled != state.unpausedTime)
        PlaySoundRPitchDirectional(enemyHit, e->position);
    if(e->health < 1) { 
        e->alive = false; 
        score += 10;
        curEnemies--;
        e->OnDeath(e);
    }
    lastTarget = e;
    lastCalled = state.unpausedTime;
    //puts(TextFormat("Enemy damaged by %d", dmg));
}

void DamageEnemiesRadius(Vector3 center, float radius, int dmg) {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(Enemies[i].alive && 
        Vector3Distance(center, (Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}) < radius) {
            DamageEnemy(&Enemies[i], dmg);
        }
    }
}

void DamagePlayer(uint dmg) {
    playerHealth -= dmg;
    PlaySoundRPitch(playerHit);
    if(playerHealth < 0) {
        state.UpdateFunc = &UpdateGameOver;
        state.DrawFunc = &DrawGameOver;
    }
}

void OnShootLaser(void) {
    PlaySoundRPitch(revShoot);
    Ray laserRay = {
        .direction = Vector3Normalize(Vector3Subtract(cam.target, cam.position)),
        .position = cam.position,
    };
    for(uint i = 0; i < MAX_ENEMIES; i++) {
        if(!Enemies[i].alive) { continue; }
        RayCollision colInfo = GetRayCollisionSphere(laserRay, (Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, 0.75f);
        if(colInfo.hit) { 
            DamageEnemy(&Enemies[i], Weapons[selectedWeapon].damage); 
        }
    }
    debugRays[0] = laserRay;
}

void OnShootLauncher(void) {
    SpawnProjectile(playerPos.x, playerPos.y, 
        Vector3Normalize(Vector3Subtract(cam.target, cam.position)), Weapons[selectedWeapon].damage, 13);
}

void OnShootShotgun(void) {
    PlaySoundRPitch(sgunShoot);
    Ray shotRay = {
        .direction = Vector3Normalize(Vector3Subtract(cam.target, cam.position)),
        .position = cam.position,
    };
    Vector3 origDir = shotRay.direction;
    Vector3 spread = Vector3Perpendicular(shotRay.direction);
    for(int j = 0; j < 8; j++) {
        Enemy* target = NULL;
        int i = 0;
        while (i < MAX_ENEMIES)
        {
            if(!Enemies[i].alive) { ++i; continue; }
            RayCollision colInfo = GetRayCollisionSphere(shotRay, (Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, 0.75f);
            if(colInfo.hit) { 
                target = &Enemies[i];
                break;
            }
            ++i;
        }
        if(target) {
            while (i < MAX_ENEMIES)
            {
                if(!Enemies[i].alive) { ++i; continue; }
                RayCollision colInfo = GetRayCollisionSphere(shotRay, (Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, 1);
                if(colInfo.hit) { 
                    if(Vector2Distance(playerPos, target->position) < Vector2Distance(playerPos, Enemies[i].position)) { ++i; continue; }
                    target = &Enemies[i];
                }
                ++i;
            }
            DamageEnemy(target, Weapons[selectedWeapon].damage);
        }
        debugRays[j] = shotRay;
        shotRay.direction = Vector3Add(Vector3Scale(spread, ((float)GetRandomValue(1, 10)/100.0f)),origDir);
        shotRay.direction = Vector3RotateByAxisAngle(shotRay.direction, origDir, (float)GetRandomValue(0, 360)*DEG2RAD);
    }
}

void OnAttackAmogus(void) {
    DamagePlayer(3);
}

void OnDeathAmogus(Enemy* e) {
    if(!GetRandomValue(0, 4)) {
        SpawnRandomItem(GetRandomValue(0, 2), e->position.x, e->position.y);
    }
    //SpawnAmmo(WT_Pistol, 10, e->position.x, e->position.y);
}

void PlaySoundRPitch(Sound sound) {
    float pitch = (float)GetRandomValue(90, 110) / 100.0f;
    SetSoundPitch(sound, pitch);
    SetSoundVolume(sound, 0.5f);
    PlaySoundMulti(sound);
}

void PlaySoundRPitchDirectional(Sound sound, Vector2 source) {
    float pitch = (float)GetRandomValue(90, 110) / 100.0f;
    SetSoundPitch(sound, pitch);
    SetSoundVolume(sound, Clamp(1.0f - Vector2Distance(playerPos, source)/50.0f, 0.0f, 1.0f));
    PlaySoundMulti(sound);
}

int GetFreeId(void* start, int max, size_t elSize) {
    const unsigned char* p = start;
    for(int i = 0; i < max; i++) {
        if(!*(bool*)p) { return i; }
        p += elSize;
    }
    return -1;
}

inline static int GetFreeEnemyId(void) {
    return GetFreeId(Enemies, MAX_ENEMIES, sizeof(Enemy));
}

inline static int GetFreePropId(void) {
    return GetFreeId(Props, MAX_PROPS, sizeof(Prop));
}

inline static int GetFreeProjectileId(void) {
    return GetFreeId(Projectiles, MAX_PROJECTILES, sizeof(Projectile));
}

inline static int GetFreeItemId(void) {
    return GetFreeId(Items, MAX_ITEMS, sizeof(Item));
}
//added bad id checks
#pragma region Spawn
void SpawnEnemy(int type, float x, float y) {
    int id = GetFreeEnemyId();
    if(id < 0) { return; }
    Enemy* e = &Enemies[id];
    e->alive = true;
    e->position = (Vector2) {x, y};
    e->spriteRect = (Rectangle) {0, 0 + type * 120 * 2, 120, 120};
    e->curFrame = 0;
    e->frameTimer = 0;
    e->state = ES_Wander;
    switch (type)
    {
    case ET_Amogus:
        e->frames = 3;
        e->frameTime = 0.4f;
        e->health = 100 + GetRandomValue(10, 50);
        e->attackRange = 1.0f;
        e->detectRange = 20.0f;
        e->speed = 5;
        e->OnAttack = &OnAttackAmogus;
        e->OnDeath = &OnDeathAmogus;
        break;
    
    default:
        e->frames = 3;
        e->frameTime = 0.4f;
        e->health = 100 + GetRandomValue(10, 50);
        e->attackRange = 1.0f;
        e->detectRange = 20.0f;
        e->speed = 5;
        e->OnAttack = &OnAttackAmogus;
        e->OnDeath = &OnDeathAmogus;
        break;
    }
}

void SpawnProjectile(float x, float y, Vector3 velocity, int dmg, uint spd) {
    int id = GetFreeProjectileId();
    if(id < 0 ) { return; }
    Projectile* b = &Projectiles[id];
    b->active = true;
    b->velocity = velocity;
    b->speed = spd;
    b->position = (Vector3){x, 1, y};
    b->damage = dmg;
}

void SpawnProp(int id, float x, float y) {
    int jd = GetFreePropId();
    if(jd < 0) { return; }
    Prop* p = &Props[jd];
    p->active = true;
    p->position = (Vector3) {x, 1, y};
    float xx, yy;
    yy = (id / (texProps.width / 64));
    xx = id - yy * (texProps.width / 64);
    p->spriteRect = (Rectangle) {
        .height = 64,
        .width = 64,
        .y = yy * 64,
        .x = xx * 64,
    };
}

Item* SpawnItem(int id, float x, float y) {
    int jd = GetFreeItemId();
    if(jd < 0) { return NULL; }
    Item* i = &Items[jd];
    i->active = true;
    i->position = (Vector3) {x, 1, y};
    float xx, yy;
    yy = (id / (texItems.width / 64));
    xx = id - yy * (texItems.width / 64);
    i->spriteRect = (Rectangle) {
        .height = 64,
        .width = 64,
        .y = yy * 64,
        .x = xx * 64,
    };
    return i;
}

void SpawnAmmo(int weapon, int amount, float x, float y) {
    Item* i = SpawnItem(AMO_OFFSET + weapon, x, y);
    if(!i) { return; }
    int* d = (int*)malloc(sizeof(int)*2);
    d[0] = weapon;
    d[1] = amount;
    i->data = d;
    i->OnPickUp = &OnPickUpAmmo;
}

void SpawnWeapon(int weapon, float x, float y) {
    Item* i = SpawnItem(WEP_OFFSET + weapon, x, y);
    if(!i) { return; }
    int* d = (int*)malloc(sizeof(int));
    d[0] = weapon;
    i->data = d;
    i->OnPickUp = &OnPickUpWeapon;
}

void SpawnMedkit(int hp, float x, float y) {
    Item* i = SpawnItem(MKT_OFFSET, x, y);
    if(!i) { return; }
    int* d = (int*)malloc(sizeof(int));
    d[0] = hp;
    i->data = d;
    i->OnPickUp = &OnPickUpMedkit;
}

void SpawnMaxHP(int hp, float x, float y) {
    Item* i = SpawnItem(MHP_OFFSET, x, y);
    if(!i) { return; }
    int* d = (int*)malloc(sizeof(int));
    d[0] = hp;
    i->data = d;
    i->OnPickUp = &OnPickUpMaxHP;
}

void SpawnAmmoBag(int weapon, int amount, float x, float y) {
    Item* i = SpawnItem(BAG_OFFSET, x, y);
    if(!i) { return; }
    int* d = (int*)malloc(sizeof(int)*2);
    d[0] = weapon;
    d[1] = amount;
    i->data = d;
    i->OnPickUp = &OnPickUpAmmoBag;
}

void SpawnRandomItem(int mod, float x, float y) {
    int luck = GetRandomValue(0, 2);
    switch (luck + mod)
    {
    case 0:
        SpawnAmmo(GetRandomValue(0, WT_LAST_ENTRY-1), GetRandomValue(10, 40), x, y);
        break;
    case 1:
        SpawnMedkit(GetRandomValue(25, 40), x, y);
        break;
    case 2:
        SpawnAmmoBag(GetRandomValue(0, WT_LAST_ENTRY-1), GetRandomValue(10, 40), x, y);
        break;
    case 3:
        SpawnMaxHP(GetRandomValue(25, 40), x, y);
        break;
    case 4:
        SpawnWeapon(GetRandomValue(0, WT_LAST_ENTRY-1), x, y);
        break;
    default:
        SpawnAmmo(GetRandomValue(0, WT_LAST_ENTRY-1), GetRandomValue(10, 40), x, y);
        break;
    }
}
#pragma endregion
void DeleteItem(Item* item) {
    item->active = false;
    free(item->data);
}

void DeleteItems(void) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        if(!Items[i].active) { continue; }
        DeleteItem(&Items[i]);
    }
}
#pragma region Assets
void LoadAssets(void) {
    texEnemies = LoadTexture("assets/textures/enemies.png");
    revShoot = LoadSound("assets/sfx/rev.wav");
    nadeExplosion = LoadSound("assets/sfx/nade.wav");
    sgunShoot = LoadSound("assets/sfx/sgun.wav");
    enemyHit = LoadSound("assets/sfx/enemyHit.wav");
    playerHit = LoadSound("assets/sfx/playerHit.wav");
    itemPickUp = LoadSound("assets/sfx/pickup.wav");
    texWeapons = LoadTexture("assets/textures/weapons.png");
    texProps = LoadTexture("assets/textures/props.png");
    texGround = LoadTexture("assets/textures/ground.png");
    texItems = LoadTexture("assets/textures/items.png");
    GenTextureMipmaps(&texGround);
    lightingTexture = LoadRenderTexture(MAP_SIZE*4, MAP_SIZE*4);
    texLight = LoadTexture("assets/textures/light0.png");
    SetTextureFilter(texLight, TEXTURE_FILTER_BILINEAR);
    combinedTexture = LoadRenderTexture(MAP_SIZE*4, MAP_SIZE*4);
    mdSkybox = LoadModelFromMesh(GenMeshCube(1,1,1));
    Image img = LoadImage("assets/textures/skyboxx.png");
    mdSkybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);
    UnloadImage(img);
    mdSkybox.materials[0].shader = LoadShader("assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
    SetShaderValue(mdSkybox.materials[0].shader, GetShaderLocation(mdSkybox.materials[0].shader, "environmentMap"), (int[1]){ MATERIAL_MAP_CUBEMAP }, SHADER_UNIFORM_INT);
    lightShader = LoadShader("assets/shaders/prop.vs", "assets/shaders/prop.fs");
    lightmapULoc = GetShaderLocation(lightShader, "texLightmap");
    SetShaderValueTexture(lightShader, lightmapULoc, lightingTexture.texture);
    for(int i = 0; i < 3; i++) {
        lvl[i] = LoadMusicStream(TextFormat("assets/sfx/music/lvl%d.mp3", i+1));
    }
}

void UnloadAssets(void) {
    UnloadTexture(texEnemies);
    UnloadSound(revShoot);
    UnloadSound(nadeExplosion);
    UnloadSound(sgunShoot);
    UnloadSound(enemyHit);
    UnloadSound(playerHit);
    UnloadSound(itemPickUp);
    UnloadTexture(texWeapons);
    UnloadTexture(texProps);
    UnloadTexture(texGround);
    UnloadTexture(texItems);
    UnloadRenderTexture(lightingTexture);
    UnloadTexture(texLight);
    UnloadRenderTexture(combinedTexture);
    UnloadModel(mdSkybox);
    UnloadShader(lightShader);
    for(int i = 0; i < 3; i++) {
        UnloadMusicStream(lvl[i]);
    }
}
#pragma endregion
#pragma region Render
Vector2 MapCoordToLightCoord(float x, float y) {
    return (Vector2){(x + MAP_SIZE/2) * 4, (y + MAP_SIZE/2) * 4};
}

void RenderLightTexture(void) {
    BeginTextureMode(lightingTexture); 
    {
        float scale = 5.3;
        float colorscale = scale * 1.2;
        DrawRectangle(0, 0, MAP_SIZE*4, MAP_SIZE*4, GetColor(0x01021aFF));
        Vector2 plp = MapCoordToLightCoord(playerPos.x, playerPos.y);
        DrawTextureEx(texLight, Vector2Subtract((Vector2){plp.x, plp.y} , (Vector2){16*20,16*20}), 0, 20, GetColor(0x22223222));
        DrawTextureEx(texLight, Vector2Subtract((Vector2){plp.x, plp.y} , (Vector2){16*scale,16*scale}), 0, scale, GetColor(0x99999944));
        for(int i = 0; i < MAX_PROJECTILES; i++) {
            if(!Projectiles[i].active) { continue; }
            Vector2 p = MapCoordToLightCoord(Projectiles[i].position.x, Projectiles[i].position.z);
            float s = Clamp(0.0f + Projectiles[i].position.y/2.0f, 2.5f, 15.0f);
            DrawTextureEx(texLight, Vector2Subtract(p, (Vector2){16.0f*s,16.0f*s}), 0, s, GetColor(0xAAAAAA77));
        }
        for(int i = 0; i < MAX_ITEMS; i++) {
            if(!Items[i].active) { continue; }
            Vector2 p = MapCoordToLightCoord(Items[i].position.x, Items[i].position.z);
            DrawTextureEx(texLight, Vector2Subtract(p, (Vector2){16.0f*1.0f,16.0f*1.0f}), 0, 1, GetColor(0xAAAAAA77));
        }
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTextureEx(texLight, Vector2Subtract((Vector2){plp.x, plp.y} , (Vector2){16*colorscale,16*colorscale}), 0, colorscale, GetColor(0xAAAAAAAA) );
        EndBlendMode();
    }
    EndTextureMode();
    BeginTextureMode(combinedTexture); 
    {
        DrawTextureTiled(texGround, (Rectangle) {0, 0, 512, 512}, (Rectangle) {0, 0, MAP_SIZE*4, MAP_SIZE*4}, (Vector2) {0, 0}, 0, 1, WHITE);
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureQuad(lightingTexture.texture, (Vector2){1,-1}, (Vector2){0,0}, (Rectangle){0,0,MAP_SIZE*4,MAP_SIZE*4}, WHITE);
        EndBlendMode();
    }
    EndTextureMode();
}

void DrawSkybox(void) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
        DrawModel(mdSkybox, (Vector3){0, 0, 0}, 1.0f, WHITE);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawEnemies(void) {
    for(int i = 0; i < curMaxEnemies; i++) {
        if(!Enemies[i].alive) { continue; }
        DrawBillboardRec(cam, texEnemies, Enemies[i].spriteRect, 
            (Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, 
            (Vector2){1,1}, WHITE);
        //DrawSphereWires((Vector3){Enemies[i].position.x, 1, Enemies[i]. position.y},0.75f,6,6,YELLOW);
    }
}

void DrawProps(void) {
    for(int i = 0; i < MAX_PROPS; i++) {
        if(!Props[i].active) { continue; }
        DrawBillboardRec(cam, texProps, Props[i].spriteRect,
        Props[i].position, (Vector2){2,2}, WHITE);
    }
}

void DrawItems(void) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        if(!Items[i].active) { continue; }
        DrawBillboardRec(cam, texItems, Items[i].spriteRect,
        Items[i].position, (Vector2) {1,1}, WHITE);
    }
}

void DrawProjectiles(void) {
    for(int i = 0; i < MAX_PROJECTILES; i++) {
        if(!Projectiles[i].active) { continue; }
        Vector3 tmp = Vector3Add(Projectiles[i].position, Vector3Scale(Projectiles[i].velocity, Projectiles[i].speed));
        Ray r;
        r.position = Projectiles[i].position;
        r.direction = Projectiles[i].velocity;
        //DrawRay(r, RED);
        DrawSphere(Projectiles[i].position, 0.5f, BLUE);
        //DrawSphereWires(Projectiles[i].position, 0.5f, 5, 5, BLUE);
    }
}

void DrawScene(void) {
    DrawCubeTexture(combinedTexture.texture, (Vector3){0, 0, 0}, MAP_SIZE, 0.1f, MAP_SIZE,WHITE);
    SetShaderValueTexture(lightShader, lightmapULoc, lightingTexture.texture);
    BeginShaderMode(lightShader);
    DrawProps();
    DrawItems();
    DrawEnemies();
    EndShaderMode();
    DrawProjectiles();
}

void DrawWeapon(void) {
    DrawTexturePro(texWeapons, 
        Weapons[selectedWeapon].spriteRect, 
        (Rectangle){GetScreenWidth()/2 - Weapons[selectedWeapon].spriteRect.width*4 / 2, 
            GetScreenHeight() - 500, Weapons[selectedWeapon].spriteRect.width*4, 500},
        Vector2Zero(), 0, WHITE);
}

void DrawUI(void) {
    //DrawText(TextFormat("%f %f %f",cam.position.x,cam.position.y,cam.position.z), 220, 40, 20, GRAY);
    DrawFPS(10, 10);
    const char* text;
    DrawText(TextFormat("Ammo: %d/%d", Weapons[selectedWeapon].ammo, Weapons[selectedWeapon].ammoCap), 10, GetScreenHeight()-20, 20, WHITE);
    DrawText(TextFormat("Health: %d/%d", playerHealth, playerHealthMax), 10, GetScreenHeight()-40, 20, WHITE);
    text = TextFormat("Enemies Remaining: %d", curEnemies);
    DrawText(text, GetScreenWidth()-MeasureText(text,20) - 10, GetScreenHeight()-20, 20, WHITE);
    text = TextFormat("Wave: %d", curWave + 1);
    DrawText(text, GetScreenWidth()-MeasureText(text,20) - 10, GetScreenHeight()-40, 20, WHITE);
    text = TextFormat("SCORE: %d", score);
    DrawText(text, GetScreenWidth()/2-MeasureText(text,40)/2, 10, 40, WHITE);
    DrawCircleLines(GetScreenWidth()/2, GetScreenHeight()/2, 10, LIME);
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!Enemies[i].alive) { continue; }
        Vector3 a = Vector3Normalize(Vector3Subtract((Vector3){cam.target.x, 1, cam.target.z}, cam.position));
        Vector3 b = Vector3Normalize(Vector3Subtract((Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, cam.position));
        Vector2 p = GetWorldToScreen((Vector3){Enemies[i].position.x, 1, Enemies[i].position.y}, cam);
        DrawCircle(p.x, 60, 6, RAYWHITE);
        if(Vector3Angle(a, b) * RAD2DEG < 90)
            DrawCircle(p.x, 60, 5, RED);
        else
            DrawCircle(p.x, 60, 5, DARKBROWN);
    }
}

void Draw(void) {
    BeginDrawing();
        ClearBackground(RAYWHITE);
        RenderLightTexture();
        BeginMode3D(cam);
            DrawSkybox();
            DrawScene();
            //for(int i = 0; i<8; i++) {
            //    DrawRay(debugRays[i], RED);
            //}
        EndMode3D();
        DrawWeapon();
        DrawUI();
    EndDrawing();
}

void DrawGameOver(void) {
    BeginDrawing();
        ClearBackground(MAROON);
        const char* text;
        text = TextFormat("YOUR SCORE: %d", score);
        DrawText(text, GetScreenWidth()/2-MeasureText(text,40)/2, GetScreenHeight()/2+50, 40, BLACK);
        text = "GAME OVER";
        DrawText(text, GetScreenWidth()/2-MeasureText(text,40)/2, GetScreenHeight()/2, 40, BLACK);
    EndDrawing();
}

void DrawWin(void) {
    BeginDrawing();
        ClearBackground(LIGHTGRAY);
        const char* text;
        text = TextFormat("YOUR SCORE: %d", score);
        DrawText(text, GetScreenWidth()/2-MeasureText(text,40)/2, GetScreenHeight()/2+50, 40, RAYWHITE);
        text = "YOU WON!";
        DrawText(text, GetScreenWidth()/2-MeasureText(text,40)/2, GetScreenHeight()/2, 40, RAYWHITE);
    EndDrawing();
}
#pragma endregion
#pragma region Update
void UpdateWeapon(void) {
    int key = GetKeyPressed();
    if(key >= KEY_ONE && key <= KEY_ONE + WT_LAST_ENTRY - 1) {
        if(Weapons[key - KEY_ONE].unlocked) {
            selectedWeapon = key - KEY_ONE;
        }
    }
    Weapon *wep = &Weapons[selectedWeapon];
    if(wep->curFrame) {
        wep->frameTimer += state.deltaTime;
        if(wep->frameTimer > wep->frameTime) {
            wep->frameTimer = 0;
            wep->curFrame--;
            wep->spriteRect.x -= (wep->spriteRect.width);
        }
    }
    else if(wep->ammo && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { 
        wep->curFrame = wep->frames - 1;
        wep->spriteRect.x = (wep->frames - 1) * (wep->spriteRect.width);
        wep->ammo--;
        wep->OnShoot();
    }
}

void UpdateView(void) {
    //Camera rotation and bobbing
    rotation.y -= mouseDelta.x * mouseSensitivity.x * state.deltaTime;
    if (rotation.y > 360)
        rotation.y -= 360;
    else if (rotation.y < 0)
        rotation.y += 360;
    rotation.x += mouseDelta.y * mouseSensitivity.y * state.deltaTime;
    rotation.x = Clamp(rotation.x, -80, 80);
    Quaternion Q = QuaternionMultiply(
        QuaternionFromAxisAngle((Vector3){0, 1, 0}, rotation.y * DEG2RAD),
        QuaternionFromAxisAngle((Vector3){1, 0, 0}, rotation.x * DEG2RAD));
    float camBobbing = sin(10 * state.unpausedTime) * Vector2Length(playerVel) * 0.01;
    cam.position = (Vector3){playerPos.x, 1 + camBobbing, playerPos.y};
    //Vector2 oldTarget = (Vector2){cam.target.x, cam.target.z};
    cam.target = Vector3Add(
        cam.position,
        Vector3RotateByQuaternion((Vector3){0, 0, 1}, Q));
    //cam.target = Vector3Lerp((Vector3){oldTarget.x, cam.target.y, oldTarget.y}, cam.target, state.deltaTime * 20);
    //Player movement
    Vector2 oldVel = playerVel;
    playerVel = (Vector2){IsKeyDown(KEY_A) - IsKeyDown(KEY_D), IsKeyDown(KEY_W) - IsKeyDown(KEY_S)};
    playerVel = Vector2Normalize(playerVel);
    playerVel.x *= playerSpeed;
    playerVel.y *= playerSpeed;
    playerVel = Vector2Lerp(oldVel, playerVel, state.deltaTime * 20);
    playerPos = Vector2Add(Vector2Rotate((Vector2){playerVel.x * state.deltaTime, playerVel.y * state.deltaTime}, -rotation.y * DEG2RAD), playerPos);
    playerPos = Vector2Clamp(playerPos, (Vector2){-MAP_SIZE/2+28, -MAP_SIZE/2+28}, (Vector2){MAP_SIZE/2-28, MAP_SIZE/2-28});
}

void UpdateEnemy(Enemy* e) {
    if(!e->alive) { return; }
    //if(e->curFrame) {
        e->frameTimer += state.deltaTime;
        if(e->frameTimer > e->frameTime) {
            e->frameTimer = 0;
            e->curFrame--;
            if(e->curFrame > -1)
                e->spriteRect.x -= e->spriteRect.width;
        }
    //}
    e->position = Vector2Clamp(Vector2Add(e->position, 
        Vector2Scale(e->velocity, e->speed * state.deltaTime)), 
        (Vector2){-MAP_SIZE/2+28, -MAP_SIZE/2+28}, (Vector2){MAP_SIZE/2-28, MAP_SIZE/2-28});
    switch (e->state)
    {
    case ES_Wander:
        if(e->curFrame < 0) {
            if(GetRandomValue(0, 1)) { 
                e->velocity = Vector2Normalize((Vector2){GetRandomValue(-1, 1), GetRandomValue(-1, 1)}); 
            }
            e->curFrame = e->frames - 1;
            e->spriteRect.x = (e->frames - 1) * e->spriteRect.width;
        }
        if(Vector2Distance(playerPos, e->position) < e->detectRange) {
            e->state = ES_Pursue;
            e->curFrame = 0;
        }
        break;

    case ES_Pursue:
        e->velocity = Vector2Normalize(Vector2Subtract(playerPos, e->position));
        if(e->curFrame < 0) {
            e->curFrame = e->frames - 1;
            e->spriteRect.x = (e->frames - 1) * e->spriteRect.width;
        }
        if(Vector2Distance(playerPos, e->position) < e->attackRange) {
            e->state = ES_Attack;
            e->OnAttack();
            e->velocity = Vector2Zero();
            e->curFrame = 0;
            e->spriteRect.y += e->spriteRect.height;
        }
        break;

    case ES_Attack:
        if(e->curFrame < 0) {
            if(Vector2Distance(playerPos, e->position) >= e->attackRange) {
                e->state = ES_Pursue;
                e->curFrame = 0;
                e->spriteRect.y -= e->spriteRect.height;
                break;
            }
            e->curFrame = e->frames - 1;
            e->spriteRect.x = (e->frames - 1) * e->spriteRect.width;
            e->OnAttack();
        }
        break;
    
    default:
        break;
    }
    
}

void UpdateEnemies(void) {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(!Enemies[i].alive) { continue; }
        UpdateEnemy(Enemies + i);
    }
}

void UpdateItem(Item* i) {
    if(!i->active) { return; }
    i->position.y = 1.0 + sin(state.unpausedTime * 10.0) * 0.01;
    if(Vector2Distance(playerPos, (Vector2){i->position.x, i->position.z}) < 1.0f) {
        i->OnPickUp(i->data);
        DeleteItem(i);
        PlaySoundRPitch(itemPickUp);
    }
}

void UpdateItems(void) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        if(!Items[i].active) { continue; }
        UpdateItem(Items + i);
    }
}

void UpdateProjectiles(void) {
    for(int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* b = &Projectiles[i];
        if(!b->active) {continue;}
        if(b->position.y < 0 ) {
            b->active = false;
            DamageEnemiesRadius(b->position, 9.5f, b->damage);
            PlaySoundRPitch(nadeExplosion);
            continue;
        }

        for(int j = 0; j < MAX_ENEMIES; j++) {
            if(!Enemies[j].alive) { continue; }
            if(Vector3Distance(b->position, (Vector3){Enemies[j].position.x, 1, Enemies[j].position.y}) < 0.5f) {
                b->active = false;
                DamageEnemiesRadius(b->position, 9.5f, b->damage);
                PlaySoundRPitch(nadeExplosion);
            }
        }

        b->position.x += b->velocity.x * state.deltaTime * b->speed;
        b->position.y += b->velocity.y * state.deltaTime * b->speed;
        b->position.z += b->velocity.z * state.deltaTime * b->speed;
        b->velocity.y -= 0.15f * state.deltaTime;
    }
}

void Update(void) {
    UpdateMusicStream(lvl[curMusic]);
    float v = GetMusicAdaptiveVolume(&lvl[curMusic]);
    if(v < 0.001f) {
        StopMusicStream(lvl[curMusic]);
        curMusic++;
        if(curMusic>2) curMusic = 0;
        PlayMusicStream(lvl[curMusic]);
    }
    else {
        SetMusicVolume(lvl[curMusic], v);
    }
    mouseDelta = GetMouseDelta();
    state.deltaTime = GetFrameTime();
    state.unpausedTime += state.deltaTime;
    UpdateView();
    UpdateWeapon();
    UpdateEnemies();
    UpdateItems();
    UpdateProjectiles();
    if(curEnemies < 1) {
        curEnemies = curMaxEnemies += curWave * 10;
        ++curWave;
        if(curMaxEnemies > MAX_ENEMIES) {
            state.DrawFunc = &DrawWin;
            state.UpdateFunc = &UpdateWin;
            return;
        }
        for(int i = 0; i < curMaxEnemies; i++) {
            SpawnEnemy(GetRandomValue(0, MIN(curWave, ET_LAST_ENTRY-1)), GetRandomValue(-90, 90), GetRandomValue(-90, 90));
        }
        int numItems = GetRandomValue(3, 7);
        for(int i = 0; i < numItems; i++) {
            SpawnAmmo(Clamp(GetRandomValue(-3,WT_LAST_ENTRY-1), 0, WT_LAST_ENTRY-1), GetRandomValue(15, 30), 
                GetRandomValue(-MAP_SIZE/2+28, MAP_SIZE/2-28), GetRandomValue(-MAP_SIZE/2+28, MAP_SIZE/2-28));
        }
    }
}

void UpdateGameOver(void) {

}

void UpdateWin(void) {

}
#pragma endregion