#include "Helper/include.h"
#include "Helper/definition.h"
#include "Helper/Items.h"
#include "Helper/Login.h"
#include "Helper/bypass.h"
#include "Helper/Hit.h"
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string>
#include <android/log.h>
#include "ClaudeTheme.h"    // correct
#include "../ImGuiCanvasBackend.h"

#include "Lua_Buffer.cpp"

json items_data;
std::map<int, bool> Items;

int _SDK() {
    char p[PROP_VALUE_MAX];
    __system_property_get(OBFUSCATE("ro.build.version.sdk"), p);
    return atoi(p);
}

static bool opened = true;
bool ShowMenu = true;

ImFont *font2 = nullptr;
ImFont *pRegularFont = nullptr;
static lgx::ImGuiCanvasBackend g_glassUi;

ImVec4 to_vec4(float r, float g, float b, float a)
{
    return ImVec4(r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}

static string compilation_date = __DATE__;
static string compilation_time = __TIME__;

int32_t (*orig_ANativeWindow_getHeight)(ANativeWindow *window);
int32_t _ANativeWindow_getHeight(ANativeWindow *window) {
    screenHeight = orig_ANativeWindow_getHeight(window);
    return orig_ANativeWindow_getHeight(window);
}

int32_t (*orig_ANativeWindow_getWidth)(ANativeWindow *window);
int32_t _ANativeWindow_getWidth(ANativeWindow *window) {
    screenWidth = orig_ANativeWindow_getWidth(window);
    return orig_ANativeWindow_getWidth(window);
}

void (*orig_onInputEvent)(void *thiz, void *ex_ab, void *ex_ac);
void _OI(void *thiz, void *ex_ab, void *ex_ac) {
    orig_onInputEvent(thiz, ex_ab, ex_ac);
    if (initImGui) {
        ImGui_ImplAndroid_HandleInputEvent((AInputEvent*)thiz, {(float)screenWidth/(float)glWidth, (float)screenHeight/(float)glHeight});
    }
}

int (*orig_AInputQueue_getEvent)(AInputQueue*, AInputEvent**);

int _HAI(AInputQueue* q, AInputEvent** e) {
    int r = orig_AInputQueue_getEvent(q, e);
    if (r >= 0 && *e) {
        int32_t t = AInputEvent_getType(*e);

        // Volume button block REMOVED

        if (t == AINPUT_EVENT_TYPE_MOTION) {
            int32_t a = AMotionEvent_getAction(*e) & AMOTION_EVENT_ACTION_MASK;
            int32_t p = (AMotionEvent_getAction(*e) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            float x = AMotionEvent_getX(*e, p);
            float y = AMotionEvent_getY(*e, p);
            float sx = (x * glWidth) / screenWidth;
            float sy = (y * glHeight) / screenHeight;
            ImGuiIO& io = ImGui::GetIO();

            switch (a) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                io.MousePos = ImVec2(sx, sy);
                io.MouseDown[0] = true;
                break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                io.MousePos = ImVec2(sx, sy);
                io.MouseDown[0] = false;
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                io.MousePos = ImVec2(sx, sy);
                break;
            }
        }
    }
    return r;
}



bool logged = false;
const char *Gamepackage = "com.pubg.imobile";





void ApplyIPadView()
{
    static ULocalPlayer* UlocalPlayer = nullptr;
    static EAspectRatioAxisConstraint OrigView = EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
    static bool origCaptured = false;

    if (!UlocalPlayer)
    {
        UlocalPlayer = UObject::FindObject<ULocalPlayer>(
            "LocalPlayer Transient.UAEGameEngine_1.LocalPlayer_1");
    }
    if (!UlocalPlayer) return;

    // Capture the original value ONCE (shared for both on/off paths)
    if (!origCaptured)
    {
        OrigView = UlocalPlayer->AspectRatioAxisConstraint;
        origCaptured = true;
    }

    if (Cheat::Memory::IPad)
    {
        if (Cheat::localController != nullptr)
        {
            UlocalPlayer->AspectRatioAxisConstraint =
                EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
        }
    }
    else
    {
        // Restore original view when toggled off
        if (UlocalPlayer->AspectRatioAxisConstraint != OrigView)
        {
            UlocalPlayer->AspectRatioAxisConstraint = OrigView;
        }
    }
}



void DrawHUD(AHUD* HUD)
{

ApplyIPadView();

    if (Cheat::localPlayer && Cheat::localController)
    {
        int totalEnemies = 0;
        int totalBots = 0;
        auto AllActors = GetActors();

        for (auto& i : AllActors)
        {
            auto Actor = i;

            if (isObjectInvalid(Actor))
                continue;

            if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass()))
            {
                auto Player = (ASTExtraPlayerCharacter*)Actor;
                bool IsVisible = Cheat::localController->LineOfSightTo(Player, { 0, 0, 0 }, true);

                FLinearColor White;
                FLinearColor boxColor;

                if (IsVisible)
                {
                    White = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
                    boxColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.7f);
                }
                else
                {
                    White = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
                    boxColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.7f);
                }

                float Distance = Cheat::localPlayer->GetDistanceTo(Player) / 100.0f;

                if (Player->PlayerKey == Cheat::localController->PlayerKey ||
                    Player->TeamID == Cheat::localController->TeamID ||
                    Player->bDead ||
                    Player->bHidden)
                    continue;

                if (Player->bEnsure)
                    totalBots++;
                else
                    totalEnemies++;

                auto HeadPos = Player->GetBonePos("Head", {});
                FVector2D headPosSC;
                auto RootPos = Player->GetBonePos("Root", {});
                FVector2D RootPosSC;

                if (W2S(HeadPos, &headPosSC) && W2S(RootPos, &RootPosSC))
                {
                    if (Cheat::Esp::Skeleton)
					{
    					static std::vector<std::vector<std::string>> skeleton =
    					{
        					{ "Head", "neck_01", "spine_03", "spine_02", "spine_01", "pelvis" },
        					{ "neck_01", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "item_r" },
        					{ "neck_01", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "item_l" },
        					{ "pelvis", "thigh_r", "calf_r", "foot_r" },
        					{ "pelvis", "thigh_l", "calf_l", "foot_l" }
    					};

    					for (auto& boneStructure : skeleton)
    					{
        					std::string lastBone;
        for (std::string& currentBone : boneStructure)
        {
            if (!lastBone.empty())
            {
                FVector2D boneFrom, boneTo;
                if (W2S(Player->GetBonePos(lastBone.c_str(), {}), &boneFrom) &&
                    W2S(Player->GetBonePos(currentBone.c_str(), {}), &boneTo))
                {
                    HUD->DrawLine(boneFrom.X, boneFrom.Y, boneTo.X, boneTo.Y, White, 1.5f);
                }
            }
            lastBone = currentBone;
        }
    }

    // Draw circle on head as before
    FVector head3D = Player->GetBonePos("Head", {});
    FVector2D headPos2D;
    if (W2S(head3D, &headPos2D))
    {
        FVector top3D = head3D;
        top3D.Z += 15.0f;
        FVector2D top2D;
        if (W2S(top3D, &top2D))
        {
            float radius = FVector2D::Distance(headPos2D, top2D);
            DrawCircleHelper(HUD, headPos2D.X, headPos2D.Y, radius, White, 36, 1.5f);
        }
    }
}

                    float height = fabs(RootPosSC.Y - headPosSC.Y);
                    float extraTop = height * 0.10f;
                    float totalHeight = height + extraTop;
                    float width = totalHeight / 2.0f;
                    float x = headPosSC.X - width / 2.0f;
                    float y = headPosSC.Y - extraTop;

                    if (Cheat::Esp::Box)
                    {
                        Box4LineHUD(HUD, x, y, width, totalHeight, 1.2f, 0.25f / 2.0f, boxColor);
                    }

                    if (Cheat::Esp::Health)
                    {
                        float CurHP = std::max(0.f, std::min(Player->Health, Player->HealthMax));
                        float MaxHP = Player->HealthMax;
                        float KnockHealth = Player->NearDeathBreath;
                        float HealthPercentage = CurHP / MaxHP;

                        FLinearColor HPColor;
                        if (CurHP > 70.0f)
                            HPColor = FLinearColor(0.0f, 0.7f, 0.8f, 1.0f);
                        else if (CurHP > 30.0f)
                            HPColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
                        else if (CurHP > 0.0f)
                            HPColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
                        else
                        {
                            HPColor = FLinearColor(0.5f, 0.0f, 0.0f, 1.0f);
                            HealthPercentage = KnockHealth / MaxHP;
                        }

                        float BarHeight = totalHeight;
                        float BarWidth = 3.0f;
                        float spacing = width * 0.1f;

                        float BarX = x - BarWidth - spacing;
                        float BarY = y;

                        DrawFilledRectangle(HUD, FVector2D(BarX, BarY), BarWidth, BarHeight, FLinearColor(0.f, 0.f, 0.f, 0.5f));

                        float FilledHeight = BarHeight * HealthPercentage;
                        float FilledY = BarY + (BarHeight - FilledHeight);
                        DrawFilledRectangle(HUD, FVector2D(BarX, FilledY), BarWidth, FilledHeight, HPColor);
                    }

                    if (Cheat::Esp::Line)
                    {
                        float lineStartX = (float)glWidth / 2;
                        float lineStartY = 0.0f;
                        float lineEndX = headPosSC.X;
                        float lineEndY = y - 5.0f;
                        HUD->DrawLine(lineStartX, lineStartY, lineEndX, lineEndY, White, 1.5f);
                    }
                    
                    
                    if (Cheat::Esp::WeaponName)
{
    std::string weaponname = "Fist";

    if (auto WeaponManagerComponent = Player->WeaponManagerComponent)
    {
        auto PropSlot = WeaponManagerComponent->GetCurrentUsingPropSlot();
        if ((int)PropSlot.GetValue() >= 1 && (int)PropSlot.GetValue() <= 3)
        {
            if (auto CurrentWeaponReplicated = WeaponManagerComponent->CurrentWeaponReplicated)
            {
                weaponname = CurrentWeaponReplicated->GetWeaponName().ToString();
            }
        }
    }

    // Colors: bot → cyan-ish, real player → yellow (same as your original)
    FLinearColor weaponColor = Player->bEnsure
        ? FLinearColor(0.0f, 1.0f, 240.f / 255.f, 1.0f)   // IM_COL32(0, 255, 240, 255)
        : FLinearColor(1.0f, 240.f / 255.f, 0.0f, 1.0f);  // IM_COL32(255, 240, 0, 255)

    // DrawOutlinedText centers horizontally on the X you pass (isCenter = true),
    // so we don't need CalcTextSize / manual offset anymore.
    FVector2D weaponPos = { headPosSC.X, headPosSC.Y - 29.5f };

    tslFont->LegacyFontSize = 11;
    DrawOutlinedText(HUD, FString(weaponname.c_str()),
                     weaponPos, weaponColor, COLOR_BLACK, true);
    tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
}
                    
                    if (Cheat::Esp::Alert)
{
    bool shit = false;

    SDK::FVector MyPosition = Player->CurrentVehicle
        ? Player->CurrentVehicle->RootComponent->RelativeLocation
        : Player->RootComponent->RelativeLocation;

    SDK::FVector EnemyPosition = Cheat::localPlayer->CurrentVehicle
        ? Cheat::localPlayer->CurrentVehicle->RootComponent->RelativeLocation
        : Cheat::localPlayer->RootComponent->RelativeLocation;

    SDK::FVector EntityPos = WorldToRadar(
        Cheat::localController->PlayerCameraManager->CameraCache.POV.Rotation.Yaw,
        MyPosition, EnemyPosition, NULL, NULL,
        Vector3((float)glWidth, (float)glHeight, 0), shit);

    SDK::FVector angle = SDK::FVector();

    Vector3 forward = Vector3(
        (float)(glWidth  / 2) - EntityPos.X,
        (float)(glHeight / 2) - EntityPos.Y,
        0.0f);
    VectorAnglesRadar(forward, angle);

    const auto angle_yaw_rad = DEG2RAD(angle.Y + 180.f);
    const auto new_point_x = (glWidth  / 2) + (30) / 2 * 4 * cosf(angle_yaw_rad);
    const auto new_point_y = (glHeight / 2) + (30) / 2 * 4 * sinf(angle_yaw_rad);

    if (IsVisible)
    {
        // ImU32 IM_COL32(0, 255, 200, 255) → FLinearColor(0, 1, 200/255, 1)
        FLinearColor alertColor = FLinearColor(0.0f, 1.0f, 200.f / 255.f, 1.0f);

        // ImGui: AddCircle(center, radius=5.0f, color, segments=64, thickness=5.0f)
        // HUD:   DrawCircleHelper(HUD, cx, cy, radius, color, segments, thickness)
        DrawCircleHelper(HUD, new_point_x, new_point_y, 5.0f, alertColor, 64, 5.0f);
    }
}
            
            
            

                    if (Cheat::Esp::Name || Cheat::Esp::Distance)
                    {
                        tslFont->LegacyFontSize = 10;

                        float textX = x + (width / 2.0f);
                        float textY = y + totalHeight + 4.0f;

                        if (Cheat::Esp::Name)
                        {
                            if (!Player->bEnsure)
                                DrawOutlinedText(HUD, Player->PlayerName, FVector2D(textX, textY), COLOR_WHITE, COLOR_BLACK, true);
                            else
                                DrawOutlinedText(HUD, FString("Bot"), FVector2D(textX, textY), COLOR_WHITE, COLOR_BLACK, true);

                            textY += 14.0f;
                        }

                        if (Cheat::Esp::Distance)
                        {
                            std::string distanceStr = std::to_string((int)Distance) + " M";
                            DrawOutlinedText(HUD, FString(distanceStr.c_str()), FVector2D(textX, textY), COLOR_YELLOW, COLOR_BLACK, true);
                        }

                        tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
                    }
                }
            }
            
            
            
            

            if (Cheat::Esp::Vehicle::Name && i->IsA(ASTExtraVehicleBase::StaticClass()))
            {
                auto Vehicle = (ASTExtraVehicleBase*)i;
                if (!Vehicle->Mesh)
                    continue;

                float Distance = Vehicle->GetDistanceTo(Cheat::localPlayer) / 100.f;

                FVector2D vehiclePos;
                if (W2S(Vehicle->K2_GetActorLocation(), &vehiclePos))
                {
                    float mWidthScale = std::min(0.10f * Distance, 50.f);
                    float mWidth = 70.f - mWidthScale;

                    std::string nameStr = GetVehicleName(Vehicle);
                    std::string distStr = std::to_string((int)Distance) + "M";

                    FLinearColor WhiteColor(1.0f, 1.0f, 0.0f, 1.0f);
                    tslFont->LegacyFontSize = 10;

                    DrawOutlinedText(HUD, FString(nameStr.c_str()), { vehiclePos.X - (mWidth / 2), vehiclePos.Y }, WhiteColor, COLOR_BLACK, true);
                    DrawOutlinedText(HUD, FString(distStr.c_str()), { vehiclePos.X - (mWidth / 2), vehiclePos.Y + 14 }, WhiteColor, COLOR_BLACK, true);

                    tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
                }
            }
			
			if (Cheat::Esp::Throwable)
            {
                if (Actor->IsA(ASTExtraGrenadeBase::StaticClass()))
                {
                    auto Grenade = (ASTExtraGrenadeBase*)Actor;
                    if (!Grenade->RootComponent)
                        continue;

                    float Distance = Grenade->GetDistanceTo(Cheat::localPlayer) / 100.f;
                    if (Distance > 50.f)
                        continue;

                    FVector2D grenadePos;
                    if (W2S(Grenade->K2_GetActorLocation(), &grenadePos))
                    {
                        tslFont->LegacyFontSize = 10;

                        DrawOutlinedText(HUD, FString("Nade"), FVector2D(grenadePos.X, grenadePos.Y),
                                         FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), COLOR_BLACK, true);

                        std::string distStr = " " + std::to_string((int)Distance) + "M";
                        DrawOutlinedText(HUD, FString(distStr.c_str()), FVector2D(grenadePos.X, grenadePos.Y + 14),
                                         FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), COLOR_BLACK, true);

                        tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
                    }
                }
            }

            if (Cheat::Esp::LootBox)
            {
                if (Actor->IsA(APickUpListWrapperActor::StaticClass()))
                {
                    auto Pick = (APickUpListWrapperActor*)Actor;
                    if (!Pick->RootComponent)
                        continue;

                    float Distance = Pick->GetDistanceTo(Cheat::localPlayer) / 100.f;
                    if (Distance > 50.f)
                        continue;

                    FVector2D boxPos;
                    if (W2S(Pick->K2_GetActorLocation(), &boxPos))
                    {
                        tslFont->LegacyFontSize = 10;

                        DrawOutlinedText(HUD, FString("Death Box"), FVector2D(boxPos.X, boxPos.Y),
                                         FLinearColor(0.0f, 1.0f, 0.0f, 1.0f), COLOR_BLACK, true);

                        std::string distStr = " " + std::to_string((int)Distance) + "M";
                        DrawOutlinedText(HUD, FString(distStr.c_str()), FVector2D(boxPos.X, boxPos.Y + 14),
                                         FLinearColor(0.0f, 1.0f, 0.0f, 1.0f), COLOR_BLACK, true);

                        tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
                    }
                }
            }
            
    

            if (Actor->IsA(APickUpWrapperActor::StaticClass()))
            {
                auto PickUp = (APickUpWrapperActor*)Actor;

                if (Items[PickUp->DefineID.TypeSpecificID])
                {
                    auto RootComponent = PickUp->RootComponent;
                    if (!RootComponent)
                        continue;

                    float Distance = PickUp->GetDistanceTo(Cheat::localPlayer) / 100.f;
                    if (Distance > 100.0f)
                        continue;

                    FVector2D itemPos;
                    if (W2S(PickUp->K2_GetActorLocation(), &itemPos))
                    {
                        std::string itemName;
                        uint32_t textColor = 0xFFFFFFFF;

                        for (auto& category : items_data)
                        {
                            for (auto& item : category["Items"])
                            {
                                if (item["itemId"] == PickUp->DefineID.TypeSpecificID)
                                {
                                    itemName = item["itemName"].get<std::string>();
                                    textColor = strtoul(item["itemTextColor"].get<std::string>().c_str(), 0, 16);
                                    break;
                                }
                            }
                        }

                        tslFont->LegacyFontSize = 10;

                        DrawOutlinedText(HUD, FString(itemName.c_str()), FVector2D(itemPos.X, itemPos.Y),
                                         UIntToLinearColor(textColor), COLOR_BLACK, true);

                        std::string distText = " " + std::to_string((int)Distance) + " M";
                        uint32_t distanceColor = 0xFFAAAAAA;
                        DrawOutlinedText(HUD, FString(distText.c_str()), FVector2D(itemPos.X, itemPos.Y + 12.0f),
                                         UIntToLinearColor(distanceColor), COLOR_BLACK, true);

                        tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
                    }
                }
            }
        }
        
        
        if (Cheat::Esp::Counter)
        {
            int totalEntities = totalEnemies + totalBots;
            if (totalEntities > 0)
            {
                std::string s = "  " + std::to_string(totalEntities);
                tslFont->LegacyFontSize = 20;
                DrawOutlinedText(HUD, FString(s), FVector2D((float)glWidth / 2.0f - 1, 110),
                                 COLOR_RED, COLOR_BLACK, true);
                tslFont->LegacyFontSize = TSL_FONT_DEFAULT_SIZE;
            }
        }
    }
}

void DrawMemory()
{
    if (Cheat::localPlayer && Cheat::localController)
    {
        if (Cheat::Aimbot::Enable) {
    // 获取瞄准目标
    ASTExtraPlayerCharacter *Target = GetTargetForAimBot();
    // 目标有效时的处理
    if (Target) {
    bool triggerOk = false;
   if (Cheat::Aimbot::Trigger == EAimTrigger::None) {
triggerOk = Cheat::localPlayer->bIsWeaponFiring;
}
if (Cheat::Aimbot::Trigger == EAimTrigger::Scoping) {
triggerOk = Cheat::localPlayer->bIsGunADS;
}
if (Cheat::Aimbot::Trigger == EAimTrigger::Both) {
triggerOk = Cheat::localPlayer->bIsWeaponFiring || Cheat::localPlayer->bIsGunADS;
}
if (triggerOk) {
FVector targetAimPos;
 if (Cheat::Aimbot::Target == EAimTarget::Head) {
 targetAimPos = Target->GetBonePos("Head", {0, 0, 0});
  }

 if (Cheat::Aimbot::Target == EAimTarget::Chest) {
 targetAimPos = Target->GetBonePos("upperarm_r", {0, 0, 0});
}
switch (Cheat::Aimbot::Target == EAimTarget::Head) {
                            case 1:
                                targetAimPos = Target->GetBonePos("Head", {});
                                break;
                            case 2:
                                targetAimPos = Target->GetBonePos("pelvis", {});
                                break;
                            case 3:
                                targetAimPos = Target->GetBonePos("calf_l", {});
                                break;
                            case 4:
                                targetAimPos = Target->GetBonePos("calf_r", {});
                                break;
                            case 5:
                                targetAimPos = Target->GetBonePos("lowerarm_l", {});
                                break;
                            case 6:
                                targetAimPos = Target->GetBonePos("lowerarm_r", {});
                                break;
                            case 7:
                                targetAimPos = Target->GetBonePos("upperarm_l", {});
                                break;
                            case 8:
                                targetAimPos = Target->GetBonePos("upperarm_r", {});
                                break;
                            case 9:
                                targetAimPos = Target->GetBonePos("thigh_l", {});
                                break;
                            case 10:
                                targetAimPos = Target->GetBonePos("thigh_r", {});
                                break;
                            case 11:
                                targetAimPos = Target->GetBonePos("foot_l", {});
                                break;
                            case 12:
                                targetAimPos = Target->GetBonePos("foot_r", {});
                                break;
                            default:
                                targetAimPos = Target->GetBonePos("Head", {});
                                break;
                        }
                        if(Cheat::Aimbot::Target == EAimTarget::Chest){
                        if(算法 == 0) {
                        targetAimPos = Target->GetBonePos("Head", {});//头
                        }else if(算法 == 1) {
                        targetAimPos = Target->GetBonePos("spine_03", {});//脖子
                        }else if(算法 == 2){
                        targetAimPos = Target->GetBonePos("pelvis", {});//屁股
                        }else if(算法 == 3){
                        targetAimPos = Target->GetBonePos("calf_l", {});//左小腿
                        }else if(算法 == 4){
                        targetAimPos = Target->GetBonePos("calf_r", {});//右小腿
                        }else if(算法 == 5){
                        targetAimPos = Target->GetBonePos("lowerarm_l", {});//左小臂
                        }else if(算法 == 6){
                        targetAimPos = Target->GetBonePos("lowerarm_r", {});//右小臂
                        }else if(算法 == 7){
                        targetAimPos = Target->GetBonePos("upperarm_l", {});//左上臂
                        }else if(算法 == 8){
                        targetAimPos = Target->GetBonePos("upperarm_r", {});//右上臂
                        }else if(算法 == 9) {
                        targetAimPos = Target->GetBonePos("thigh_l", {});//左大腿
                        }else if(算法 == 10) {
                        targetAimPos = Target->GetBonePos("thigh_r", {});//右大腿
                        }else if(算法 == 11) {
                        targetAimPos = Target->GetBonePos("foot_l", {});//左脚
                        }else if(算法 == 12){
                        targetAimPos = Target->GetBonePos("foot_r", {});//右脚
                        }
                        }        
                        /*
            if (targetAimPos.X > 0 && targetAimPos.Y > 0 && targetAimPos.Z > 0) {
    auto WeaponManagerComponent = Cheat::localPlayer->WeaponManagerComponent;
    if (WeaponManagerComponent) {
        auto propSlot = WeaponManagerComponent->GetCurrentUsingPropSlot();
        if ((int) propSlot.GetValue() >= 1 && (int) propSlot.GetValue() <= 3) {
            auto CurrentWeaponReplicated = (ASTExtraShootWeapon *) WeaponManagerComponent->CurrentWeaponReplicated;
            if (CurrentWeaponReplicated) {
                auto ShootWeaponComponent = CurrentWeaponReplicated->ShootWeaponComponent;
                auto ShootWeaponEffectComp = CurrentWeaponReplicated->ShootWeaponEffectComp;
                if (ShootWeaponComponent) {
                    UShootWeaponEntity *ShootWeaponEntityComponent = ShootWeaponComponent->ShootWeaponEntityComponent;
                    if (ShootWeaponEntityComponent) {
                        // Get bullet fire speed using offset 0x408
                        float BulletFireSpeed = *(float*)((uintptr_t)ShootWeaponEntityComponent + 0x560);
                        
                        ASTExtraVehicleBase *CurrentVehicle = Target->CurrentVehicle;
                        if (CurrentVehicle) {
                            FVector LinearVelocity = CurrentVehicle->ReplicatedMovement.LinearVelocity;
                            float dist = Cheat::localPlayer->GetDistanceTo(Target);
                            auto timeToTravel = dist / BulletFireSpeed;  // Using BulletFireSpeed instead of BulletRange
                            targetAimPos = UKismetMathLibrary::Add_VectorVector(targetAimPos, UKismetMathLibrary::Multiply_VectorFloat(LinearVelocity, timeToTravel));
                            targetAimPos.Z += LinearVelocity.Z * timeToTravel + 0.5 * 573.f * timeToTravel * timeToTravel;
                        } else {
                            FVector Velocity = Target->GetVelocity();
                            float dist = Cheat::localPlayer->GetDistanceTo(Target);
                            auto timeToTravel = dist / BulletFireSpeed;  // Using BulletFireSpeed instead of BulletRange
                            targetAimPos = UKismetMathLibrary::Add_VectorVector(targetAimPos, UKismetMathLibrary::Multiply_VectorFloat(Velocity, timeToTravel));
                            targetAimPos.Z += Velocity.Z * timeToTravel + 0.5 * 573.f * timeToTravel * timeToTravel;
                        }
                        
                        
                        
                        if (Cheat::localPlayer->bIsGunADS && Cheat::localPlayer->bIsWeaponFiring) {
                                float dist = Cheat::localPlayer->GetDistanceTo(Target) / 100.f;
                                targetAimPos.Z -= dist * Cheat::Aimbot::RecoilSet;
                            }

                            if (Cheat::localPlayer->bIsWeaponFiring) {
                                // Camera position & target direction
                                FVector gunlocation = Cheat::localController->PlayerCameraManager->CameraCache.POV.Location;
                                FVector dir = targetAimPos - gunlocation;

                                // Normalize
                                float len = sqrtf(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
                                if (len > 0.001f) {
                                    dir.X /= len;
                                    dir.Y /= len;
                                    dir.Z /= len;
                                }

                                // Convert direction → pitch & yaw
                                FRotator aimrotation;
                                aimrotation.Yaw   = atan2f(dir.Y, dir.X) * (180.f / PI_F);
                                aimrotation.Pitch = atan2f(dir.Z, sqrtf(dir.X * dir.X + dir.Y * dir.Y)) * (180.f / PI_F);
                                aimrotation.Roll  = 0;

                                FRotator gunrotaton = Cheat::localController->PlayerCameraManager->CameraCache.POV.Rotation;

                                FRotator deltaRotation;
                                deltaRotation.Pitch = aimrotation.Pitch - gunrotaton.Pitch;
                                deltaRotation.Yaw   = aimrotation.Yaw - gunrotaton.Yaw;
                                deltaRotation.Roll  = 0;

                                ClampAngles(deltaRotation);

                                // Smooth aim
                                float smoothFactor = Cheat::Aimbot::Smoothing;
                                deltaRotation.Pitch /= smoothFactor;
                                deltaRotation.Yaw   /= smoothFactor;

                                Cheat::localController->AddYawInput(deltaRotation.Yaw);
                                Cheat::localController->AddPitchInput(deltaRotation.Pitch);
                        
                        }
                    }
                }
            }
        }
    }
}
*/
auto WeaponManagerComponent = Cheat::localPlayer->WeaponManagerComponent;
if (WeaponManagerComponent)
{
    auto propSlot = WeaponManagerComponent->GetCurrentUsingPropSlot();
    if ((int)propSlot.GetValue() >= 1 && (int)propSlot.GetValue() <= 3)
    {
        auto CurrentWeaponReplicated = (ASTExtraShootWeapon *)WeaponManagerComponent->CurrentWeaponReplicated;
        if (CurrentWeaponReplicated)
        {
            auto ShootWeaponComponent = CurrentWeaponReplicated->ShootWeaponComponent;
            if (ShootWeaponComponent)
            {
                UShootWeaponEntity *ShootWeaponEntityComponent = ShootWeaponComponent->ShootWeaponEntityComponent;
                if (ShootWeaponEntityComponent)
                {
                    float BulletFireSpeed = *(float*)((uintptr_t)ShootWeaponEntityComponent + 0x560);

                    ASTExtraVehicleBase *CurrentVehicle = Target->CurrentVehicle;
                    float dist = Cheat::localPlayer->GetDistanceTo(Target);
                    auto timeToTravel = dist / ShootWeaponEntityComponent->BulletFireSpeed;

                    if (CurrentVehicle)
                    {
                        FVector LinearVelocity = CurrentVehicle->ReplicatedMovement.LinearVelocity;
                        targetAimPos = UKismetMathLibrary::Add_VectorVector(
                            targetAimPos,
                            UKismetMathLibrary::Multiply_VectorFloat(LinearVelocity, timeToTravel));
                    }
                    else
                    {
                        FVector Velocity = Target->GetVelocity();
                        targetAimPos = UKismetMathLibrary::Add_VectorVector(
                            targetAimPos,
                            UKismetMathLibrary::Multiply_VectorFloat(Velocity, timeToTravel));
                    }

                    if (Cheat::localPlayer->bIsWeaponFiring)
                    {
                        float distRecoil = Cheat::localPlayer->GetDistanceTo(Target) / 100.f;
                        targetAimPos.Z -= distRecoil * Cheat::Aimbot::RecoilSet;
                    }

                    FVector fDir = UKismetMathLibrary::Subtract_VectorVector(
                        targetAimPos,
                        Cheat::localController->PlayerCameraManager->CameraCache.POV.Location);

                    FRotator Yaptr = UKismetMathLibrary::Conv_VectorToRotator(fDir);
                    FRotator CpYaT = Cheat::localController->PlayerCameraManager->CameraCache.POV.Rotation;

                    // Raw delta between desired aim and current camera
                    Yaptr.Pitch -= CpYaT.Pitch;
                    Yaptr.Yaw   -= CpYaT.Yaw;
                    Yaptr.Roll   = 0.f;

                    // 1) Normalize / clamp the raw delta FIRST (operates on true delta)
                    NekoHook(Yaptr);

                    // 2) Apply smoothing to the delta (send smoothed value to input)
                    float smoothFactor = Cheat::Aimbot::Smoothing;
                    if (smoothFactor < 1.f) smoothFactor = 1.f;   // Smoothing = 1 => instant snap

                    Yaptr.Pitch /= smoothFactor;
                    Yaptr.Yaw   /= smoothFactor;

                    // 3) Send the SMOOTHED delta
                    Cheat::localPlayer->AddControllerYawInput(Yaptr.Yaw);
                    Cheat::localPlayer->AddControllerPitchInput(Yaptr.Pitch);
                }
            }
        }
    }
}






        }
    }
}

	
        if (Cheat::Memory::Hit)
        {
            TriggerHitEffect();
        }
        
        
        if (Cheat::Memory::IslandBp)
        {
MemoryPatch::createWithHex("libhdmpve.so",0x1B7504,"C0 03 5F D6").Modify();//𝐎𝐧
MemoryPatch::createWithHex("libhdmpve.so",0x131D30,"C0 03 5F D6").Modify();//𝐎𝐧
}else{ 
MemoryPatch::createWithHex("libhdmpve.so",0x1B7504,"FF 83 00 D1").Modify();//𝐎𝐟𝐟
MemoryPatch::createWithHex("libhdmpve.so",0x131D30,"FF 83 04 D1").Modify();//𝐎𝐟𝐟
}
        if (Cheat::Memory::Small)
        {
            auto WeaponManagerComponent = Cheat::localPlayer->WeaponManagerComponent;

            if (WeaponManagerComponent)
            {
                auto propSlot = WeaponManagerComponent->GetCurrentUsingPropSlot();

                if ((int)propSlot.GetValue() >= 1 && (int)propSlot.GetValue() <= 3)
                {
                    auto CurrentWeaponReplicated = (ASTExtraShootWeapon*)WeaponManagerComponent->CurrentWeaponReplicated;

                    if (CurrentWeaponReplicated)
                    {
                        auto ShootWeaponComponent = CurrentWeaponReplicated->ShootWeaponComponent;

                        if (ShootWeaponComponent)
                        {
                            UShootWeaponEntity* ShootWeaponEntityComponent = ShootWeaponComponent->ShootWeaponEntityComponent;

                            if (ShootWeaponEntityComponent)
                            {
                            
                            if (Cheat::Memory::Small)
                                {
                                    Write<float>(Cheat::libUE4Base + 0x608FF64, 8.47963525e-21); // small cross
Write<float>(Cheat::libUE4Base + 0xA4AA724, 8.956718135072719E-21); // instant hit
                                }
                         
                          
                            }
                        }
                    }
                }
            }
        }
    }
}

void AutoEspOn() 
{
    Cheat::Esp::Line = true;
    Cheat::Esp::Name = true;
    Cheat::Esp::Distance = true;
    Cheat::Esp::Health = true;
    Cheat::Esp::Skeleton = true;
    //Cheat::Esp::Box = true;
	Cheat::Esp::LootBox = true;
	Cheat::Esp::Throwable = true;
	Cheat::Esp::Counter = true;
    Cheat::Esp::Vehicle::Name = true;
    
    
    Cheat::Aimbot::Radius = 100.0f;
    Cheat::Aimbot::RecoilSet = 1.045f;
    Cheat::Aimbot::IgnoreKnock = true;
    Cheat::Aimbot::VisCheck = true;
    
    Cheat::Aimbot::Smoothing = 4.0;
    
    Cheat::BulletTrack::TrackingAccuracy = 1;
    Cheat::Aimbot::Range = 120.0f;
   
    for (auto &i : items_data) 
    {
        int itemCount = 0;
        for (auto &item : i[("Items")]) 
        {
            item[("itemName")].get<std::string>().c_str();
            Items[item[("itemId")].get<int>()] = true;
            itemCount++;

            if (itemCount % 4 != 0 && &item != &i[("Items")].back()){}
        }
    }
}


 
EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    static bool loggedSwapHook = false;
    if (!loggedSwapHook) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                            "LiquidGlass diag: eglSwapBuffers hook entered");
        loggedSwapHook = true;
    }

    eglQuerySurface(dpy, surface, EGL_WIDTH,  &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (glWidth <= 0 || glHeight <= 0)
        return orig_eglSwapBuffers(dpy, surface);

    if (!g_App)
        return orig_eglSwapBuffers(dpy, surface);

    screenWidth  = ANativeWindow_getWidth(g_App->window);
    screenHeight = ANativeWindow_getHeight(g_App->window);
    density      = AConfiguration_getDensity(g_App->config);

    // ----------------------------------------------------------------
    // One-time init
    // ----------------------------------------------------------------
    if (!initImGui) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: beginning ImGui/GLES initialization (%dx%d)",
             glWidth, glHeight);
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // Icons config (merge into base font)
        ImFontConfig icons_cfg;
        icons_cfg.MergeMode   = true;
        icons_cfg.PixelSnapH  = true;
        icons_cfg.OversampleH = 2;
        icons_cfg.OversampleV = 2;
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        // Base UI font
        io.Fonts->AddFontFromMemoryTTF((void*)Custom_data, Custom_size, 16.0f,
                                       nullptr, io.Fonts->GetGlyphRangesDefault());

        // Icon font merged into base
        io.Fonts->AddFontFromMemoryTTF((void*)fontAwesome, sizeof(fontAwesome),
                                       15.0f, &icons_cfg, icons_ranges);

        // Small ESP/mono font (kept for existing usage)
        font2 = io.Fonts->AddFontFromMemoryTTF((void*)voidespfont,
                                               sizeof(voidespfont), 16.0f);

        // Default fallback
        ImFontConfig cfg;
        cfg.SizePixels = ((float)density / 20.0f);
        io.Fonts->AddFontDefault(&cfg);

        // Apply the Claude-style theme
        Claude::Apply();

        ImGui_ImplAndroid_Init();
        const bool openGLBackendReady =
            ImGui_ImplOpenGL3_Init(OBFUSCATE("#version 300 es"));
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: ImGui OpenGL backend init=%d", openGLBackendReady ? 1 : 0);

        // The ImGui backend uses framebuffer-pixel coordinates in this hook.
        // Initialize LiquidGlass only after the GLES3 context and ImGui renderer
        // are ready; glass draw callbacks are executed by RenderDrawData below.
        if (openGLBackendReady) {
            __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: shader startup begin");
            const bool glassReady = g_glassUi.initialize(1.0f);
            if (glassReady) {
                __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: shader startup succeeded");
            }
            else {
                __android_log_print(ANDROID_LOG_ERROR, "LiquidGlass", "LiquidGlass diag: shader startup failed; using ImGui fallback");
            }
        }
        else {
            __android_log_print(ANDROID_LOG_ERROR, "LiquidGlass", "LiquidGlass diag: skipping glass startup because ImGui GLES init failed");
        }
        initImGui = true;
    }

    static unsigned int glassDiagFrame = 0;
    const unsigned int currentGlassDiagFrame = ++glassDiagFrame;
    bool traceGlassFrame = currentGlassDiagFrame <= 3;
    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: frame %u begin", currentGlassDiagFrame);
    }

    ImGuiIO& io = ImGui::GetIO();
    g_glassUi.beginFrame(io.DeltaTime); // Required before ImGui::NewFrame().
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    ImGui::NewFrame();
    g_glassUi.syncFrame();
    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: frame %u ImGui NewFrame complete", currentGlassDiagFrame);
    }

    // ----------------------------------------------------------------
    // Persistent state
    // ----------------------------------------------------------------
    static bool ShowMenu = false;
    static bool opened   = false;

    if (!opened) {
        opened   = true;
        ShowMenu = false;
    }

    static bool lastLoggedMenuState = false;
    if (ShowMenu != lastLoggedMenuState) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: menu visibility changed to %d", ShowMenu ? 1 : 0);
        lastLoggedMenuState = ShowMenu;
        traceGlassFrame = true;
    }

    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                            "LiquidGlass diag: frame %u before toggle window",
                            currentGlassDiagFrame);
    }

    // Liquid glass is drawn behind native ImGui widgets. Transparent window and
    // child fills let the shader sample the game frame instead of a solid panel.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.70f, 0.84f, 1.0f, 0.24f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.18f, 0.25f, 0.58f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.29f, 0.41f, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.35f, 0.53f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.73f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.34f, 0.66f, 0.96f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.49f, 0.79f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.21f, 0.31f, 0.42f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.19f, 0.32f, 0.48f, 0.68f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.39f, 0.57f, 0.82f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // ----------------------------------------------------------------
    // Floating toggle window (LiquidGlass pill)
    // ----------------------------------------------------------------
    {
        static const ImGuiWindowFlags toggleFlags =
            ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse     | ImGuiWindowFlags_NoScrollbar    |
            ImGuiWindowFlags_NoTitleBar;

        ImGui::SetNextWindowSize(ImVec2(108, 56), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("##toggle", nullptr, toggleFlags)) {
            if (traceGlassFrame) {
                __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                                    "LiquidGlass diag: toggle Begin complete");
            }
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            g_glassUi.drawGlassPanel(
                lgx::Box(windowPos.x, windowPos.y,
                         windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                18.0f, 6.0f, lgx::Rgba(0.10f, 0.15f, 0.22f, 0.32f), true);
            if (traceGlassFrame) {
                __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                                    "LiquidGlass diag: toggle glass panel queued");
            }

            // Keep the hit target on native ImGui for compatibility. The
            // LiquidGlass card behind it remains shader-backed; bypassing the
            // library PillButton also isolates its first-frame crash path.
            ImGui::SetCursorPos(ImVec2(12, 10));
            if (traceGlassFrame) {
                __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                                    "LiquidGlass diag: before native toggle button");
            }
            if (ImGui::Button(ShowMenu ? "Close" : "Menu", ImVec2(84, 36)))
                ShowMenu = !ShowMenu;
            if (traceGlassFrame) {
                __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                                    "LiquidGlass diag: native toggle button complete");
            }
        }
        ImGui::End();
        if (traceGlassFrame) {
            __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                                "LiquidGlass diag: toggle window complete");
        }
    }

    // ----------------------------------------------------------------
    // Main window
    // ----------------------------------------------------------------
    if (ShowMenu) {
        ImGui::SetNextWindowSize(
            ImVec2((float)glWidth * 0.46f, (float)glHeight * 0.58f),
            ImGuiCond_Once);

        static const ImGuiWindowFlags winFlags =
            ImGuiWindowFlags_NoResize       | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse     | ImGuiWindowFlags_NoScrollbar    |
            ImGuiWindowFlags_NoTitleBar;

        if (ImGui::Begin("Anoy Hax", &opened, winFlags)) {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            g_glassUi.drawGlassPanel(
                lgx::Box(windowPos.x, windowPos.y,
                         windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                20.0f, 9.0f, lgx::Rgba(0.08f, 0.12f, 0.18f, 0.34f), true);

            // Draw the title after the glass callback so it stays crisp.
            ImGui::SetCursorPos(ImVec2(18, 13));
            ImGui::TextColored(ImVec4(0.91f, 0.96f, 1.0f, 1.0f), "Anoy Hax");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.52f, 0.70f, 0.88f, 0.92f), "LIQUID GLASS");
            ImGui::SetCursorPos(ImVec2(16, 48));

            // ---------------- Left nav ----------------
            if (ImGui::BeginChild("##nav", ImVec2(Claude::NavWidth, 0), false)) {
                const ImVec2 navPos = ImGui::GetWindowPos();
                const ImVec2 navSize = ImGui::GetWindowSize();
                g_glassUi.drawGlassPanel(
                    lgx::Box(navPos.x, navPos.y, navPos.x + navSize.x,
                             navPos.y + navSize.y),
                    14.0f, 5.0f, lgx::Rgba(0.09f, 0.13f, 0.19f, 0.27f));

                Claude::SectionHeader("Sections");
                if (Claude::NavItem("Player ESP",     Settings::Tab == 1)) Settings::Tab = 1;
                if (Claude::NavItem("World ESP",      Settings::Tab == 2)) Settings::Tab = 2;
                if (Claude::NavItem("Aimbot",         Settings::Tab == 3)) Settings::Tab = 3;
                if (Claude::NavItem("Bullet track",   Settings::Tab == 4)) Settings::Tab = 4;
                if (Claude::NavItem("Extra features", Settings::Tab == 5)) Settings::Tab = 5;
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // ---------------- Right content ----------------
            if (ImGui::BeginChild("##content", ImVec2(0, 0), false)) {
                const ImVec2 contentPos = ImGui::GetWindowPos();
                const ImVec2 contentSize = ImGui::GetWindowSize();
                g_glassUi.drawGlassPanel(
                    lgx::Box(contentPos.x, contentPos.y,
                             contentPos.x + contentSize.x, contentPos.y + contentSize.y),
                    14.0f, 5.0f, lgx::Rgba(0.09f, 0.13f, 0.19f, 0.27f));

                if (Settings::Tab == 1) {
                    Claude::SectionHeader("Player ESP");
                    ImGui::Checkbox("ESP line",     &Cheat::Esp::Line);
                    ImGui::Checkbox("ESP name",     &Cheat::Esp::Name);
                    ImGui::Checkbox("ESP distance", &Cheat::Esp::Distance);
                    ImGui::Checkbox("ESP health",   &Cheat::Esp::Health);
                    ImGui::Checkbox("ESP skeleton", &Cheat::Esp::Skeleton);
                    ImGui::Checkbox("ESP box",      &Cheat::Esp::Box);
                    ImGui::Checkbox("Alert",        &Cheat::Esp::Alert);
                    ImGui::Checkbox("Loot box",     &Cheat::Esp::LootBox);
                    ImGui::Checkbox("Counter",      &Cheat::Esp::Counter);
                    ImGui::Checkbox("Vehicle name", &Cheat::Esp::Vehicle::Name);
                }
                else if (Settings::Tab == 2) {
                    Claude::SectionHeader("World ESP");
                    ImGui::Checkbox("Throwable",    &Cheat::Esp::Throwable);
                    ImGui::Checkbox("Enemy weapon", &Cheat::Esp::WeaponName);
                    ImGui::Checkbox("Target line",  &Cheat::Esp::Target);
                }
                else if (Settings::Tab == 3) {
                    Claude::SectionHeader("Aimbot");
                    ImGui::Checkbox("Enable aimbot", &Cheat::Aimbot::Enable);
                    ImGui::Checkbox("Enable FOV",    &Cheat::Aimbot::EnableFov);
                    if (Cheat::Aimbot::EnableFov) {
                        ImGui::SliderFloat("Aim FOV",      &Cheat::Aimbot::Radius, 0.0f, 400.0f, "%.0f");
                        ImGui::Checkbox("Show FOV circle", &Cheat::Aimbot::ShowFovCircle);
                    }
                    ImGui::SliderFloat("Aim range",           &Cheat::Aimbot::Range,     0.0f, 400.0f, "%.0f m");
                    ImGui::SliderFloat("Aim smoothing",       &Cheat::Aimbot::Smoothing, 1.0f, 4.0f,   "%.0f");
                    ImGui::SliderFloat("Recoil compensation", &Cheat::Aimbot::RecoilSet, 0.0f, 2.0f);
                    ImGui::Checkbox("Visibility check", &Cheat::Aimbot::VisCheck);
                    ImGui::Checkbox("Ignore knocked",   &Cheat::Aimbot::IgnoreKnock);
                    ImGui::Checkbox("Ignore bot",       &Cheat::Aimbot::IgnoreBot);
                }
                else if (Settings::Tab == 4) {
                    Claude::SectionHeader("Bullet track");
                    ImGui::Checkbox("Enable bullet track", &Cheat::BulletTrack::Enable);
                    ImGui::Checkbox("Enable FOV",          &Cheat::Aimbot::EnableFov);
                    if (Cheat::Aimbot::EnableFov) {
                        ImGui::SliderFloat("Aim FOV",      &Cheat::Aimbot::Radius, 0.0f, 400.0f, "%.0f");
                        ImGui::Checkbox("Show FOV circle", &Cheat::Aimbot::ShowFovCircle);
                    }
                    ImGui::SliderFloat("Aim range",              &Cheat::Aimbot::Range, 0.0f, 400.0f, "%.0f m");
                    ImGui::Checkbox("Force headshot on full",    &Cheat::BulletTrack::ForceHeadshotWhenFull);
                    ImGui::SliderInt("Tracking accuracy",        &Cheat::BulletTrack::TrackingAccuracy, 1, 3, "%d/3");
                    ImGui::Checkbox("Visibility check",          &Cheat::Aimbot::VisCheck);
                    ImGui::Checkbox("Ignore knocked",            &Cheat::Aimbot::IgnoreKnock);
                    ImGui::Checkbox("Ignore bot",                &Cheat::Aimbot::IgnoreBot);
                }
                else if (Settings::Tab == 5) {
                    Claude::SectionHeader("Extra features");
                    ImGui::Checkbox("Small cross",  &Cheat::Memory::Small);
                    ImGui::Checkbox("X-hit effect", &Cheat::Memory::Hit);
                    ImGui::Checkbox("iPad view",    &Cheat::Memory::IPad);
                    ImGui::Checkbox("6-hour fixer", &Cheat::Memory::IslandBp);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass",
                            "LiquidGlass diag: frame %u UI construction complete",
                            currentGlassDiagFrame);
    }
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(12);

    ImGui::Render();
    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: frame %u entering RenderDrawData", currentGlassDiagFrame);
    }
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (traceGlassFrame) {
        __android_log_print(ANDROID_LOG_INFO, "LiquidGlass", "LiquidGlass diag: frame %u RenderDrawData complete", currentGlassDiagFrame);
    }
    g_glassUi.finishFrame();

    return orig_eglSwapBuffers(dpy, surface);
}

void (*oReceiveDrawHUD)(AHUD *pHUD, int SizeX, int SizeY);
void hkReceiveDrawHUD(AHUD *pHUD, int SizeX, int SizeY)
{
    if (pHUD)
    {
        RenderESPPRIVATE(pHUD, SizeX, SizeY);
        DrawHUD(pHUD);
        DrawMemory();
    }
    oReceiveDrawHUD(pHUD, SizeX, SizeY);
}

/*
void* (*oProcessEvent)(UObject*, UFunction*, void*);
void* hkProcessEvent(UObject* pObj, UFunction* pFunc, void* pArgs) 
{
    if (!pObj || !pFunc) 
        return oProcessEvent(pObj, pFunc, pArgs);

    const char* EngineHUD = ("Function Engine.HUD.ReceiveDrawHUD");
    if (pFunc->GetFullName() == EngineHUD) 
    {
        AHUD* pHUD = (AHUD*)pObj;
        if (pHUD) 
        {
            auto Params = (AHUD_ReceiveDrawHUD_Params*)pArgs;
            if (Params) 
            {
                RenderESPPRIVATE(pHUD, Params->SizeX, Params->SizeY);
                DrawHUD(pHUD);
                DrawMemory();
            }
        }
    }

    auto fnc = pFunc->GetFullName();
    if (Cheat::localPlayer && Cheat::localController && Cheat::Memory::ShowDamage && fnc.find("ClientOnDamageToOther") != std::string::npos) 
    {
        auto localContrller = reinterpret_cast<ASTExtraPlayerController*>(pObj);
        auto Params = reinterpret_cast<ASTExtraPlayerController_ClientOnDamageToOther_Params*>(pArgs);
        if (Params) 
        {
            float damage = Params->_DamageToOther;
            if (auto HUD = reinterpret_cast<ASurviveHUD*>(localContrller->MyHUD)) 
            {
                HUD->AddHitDamageNumberWithConfig(damage, Cheat::localPlayer, Cheat::localController, 0, 1, 1, 1);
            }
        }
    }
    return oProcessEvent(pObj, pFunc, pArgs);
}

void initOffset() 
{
    ProcessEvent = (Cheat::libUE4Base + 0x8BE9D08);
    if (ProcessEvent) 
    {
       shadowhook_init(shadowhook_mode_t::SHADOWHOOK_MODE_UNIQUE, 0);
       shadowhook_hook_func_addr((void *)(Cheat::libUE4Base + 0x8BE9D08), (void *)hkProcessEvent, (void **)&oProcessEvent);
       
    }
}

*/


void FixGameCrash()
{
    system("rm -rf /data/data/com.pubg.imobile/files");
    system("rm -rf /data/data/com.pubg.imobile/files/ano_tmp");
    system("touch /data/data/com.pubg.imobile/files/ano_tmp");
    system("chmod 000 /data/data/com.pubg.imobile/files/ano_tmp");
    system("rm -rf /data/data/com.pubg.imobile/files/obblib");
    system("touch /data/data/com.pubg.imobile/files/obblib");
    system("chmod 000 /data/data/com.pubg.imobile/files/obblib");
    system("rm -rf /data/data/com.pubg.imobile/files/xlog");
    system("touch /data/data/com.pubg.imobile/files/xlog");
    system("chmod 000 /data/data/com.pubg.imobile/files/xlog");
    system("rm -rf /data/data/com.pubg.imobile/app_bugly");
    system("touch /data/data/com.pubg.imobile/app_bugly");
    system("chmod 000 /data/data/com.pubg.imobile/app_bugly");
    system("rm -rf /data/data/com.pubg.imobile/app_crashrecord");
    system("touch /data/data/com.pubg.imobile/app_crashrecord");
    system("chmod 000 /data/data/com.pubg.imobile/app_crashrecord");
    system("rm -rf /data/data/com.pubg.imobile/app_crashSight");
    system("touch /data/data/com.pubg.imobile/app_crashSight");
    system("chmod 000 /data/data/com.pubg.imobile/app_crashSight");
}


void *RunGame(void *) 
{
FixGameCrash();

    Cheat::libUE4Base = Tools::GetBaseAddress("libUE4.so");

    while (!Cheat::libUE4Base) 
    {
        Cheat::libUE4Base = Tools::GetBaseAddress("libUE4.so");
        sleep(1);
    }

    while (!g_App) 
    {
        g_App = *(android_app **)(Cheat::libUE4Base + Cheat::GNativeAndroidApp_Offset);
        sleep(1);
    }

    FName::GNames = GetGNames();

    while (!FName::GNames) 
    {
        FName::GNames = GetGNames();
        sleep(1);
    }

    UObject::GUObjectArray = (FUObjectArray *)(Cheat::libUE4Base + Cheat::GUObject_Offset);
    
    shadowhook_init(shadowhook_mode_t::SHADOWHOOK_MODE_UNIQUE, 0);
    
    shadowhook_hook_func_addr((void *)(Cheat::libUE4Base + 0xAD5866C), (void *)hkReceiveDrawHUD, (void **)&oReceiveDrawHUD);
  
    shadowhook_hook_func_addr((void *)(Cheat::libUE4Base + 0xCC8F5E0), (void *)_eglSwapBuffers, (void **)&orig_eglSwapBuffers);
  
    //shadowhook_hook_sym_name("/system/lib64/libEGL.so", "eglSwapBuffers", (void *)_eglSwapBuffers, (void **)&orig_eglSwapBuffers);
    
    shadowhook_hook_func_addr((void *)(Cheat::libUE4Base + 0x6DAB6CC), (void *)xShootBulletInner, (void **)&ShootBulletInner);
    
    
    shadowhook_hook_sym_name("libandroid.so", "AInputQueue_getEvent",
        (void*)_HAI, (void**)&orig_AInputQueue_getEvent);

    if (_SDK() < 35) {
        shadowhook_hook_sym_name("libinput.so",
            "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE",
            (void*)_OI, (void**)&orig_onInputEvent);
    }
    if (_SDK() == 35) {
        shadowhook_hook_sym_name("libinput.so",
            "_ZN7android11MotionEvent8copyFromEPKS0_b",
            (void*)_OI, (void**)&orig_onInputEvent);
    }
    if (_SDK() == 36) {
        shadowhook_hook_sym_name("libinput.so",
            "_ZN7android11MotionEvent8copyFromEPKS0_b",
            (void*)_OI, (void**)&orig_onInputEvent);
    }

    shadowhook_hook_sym_name("libandroid.so", "ANativeWindow_getWidth",
        (void*)_ANativeWindow_getWidth, (void**)&orig_ANativeWindow_getWidth);
    shadowhook_hook_sym_name("libandroid.so", "ANativeWindow_getHeight",
        (void*)_ANativeWindow_getHeight, (void**)&orig_ANativeWindow_getHeight);
	
	//initOffset();
	InitLuaHooks(Cheat::libUE4Base);
    items_data = json::parse(JSON_ITEMS);
    AutoEspOn();
    return nullptr;
}

__attribute__ ((constructor))
void _init() 
{
    pthread_create(&t, NULL, RunGame, NULL);
}
