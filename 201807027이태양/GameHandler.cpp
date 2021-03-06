#include "GameHandler.h"
#include <iostream>
#include <utility>
#include <ctime>
#include <cstdlib>
#include <algorithm>


#include "MonsterBase.h"
#include "ButtonObject.h"
#include "ProjectileBase.h"
#include "Clock.h"
#include "PurpleDIce.h"
#include "GrayDice.h"
#include "IceDice.h"
#include "SniperDice.h"
#include "BlackDice.h"

DWORD WINAPI MonsterTr(LPVOID Param);
DWORD WINAPI DiceTr(LPVOID Param);
DWORD WINAPI PlayTr(LPVOID Param);
GameHandler* GameHandler::Instance = nullptr;
HWND GameHandler::hWnd = NULL;
//HWND g_hWnd;



GameHandler::GameHandler() : DIceUpgradeNum{ 0,0,0,0,0,0 }
{
    MousePos = { 0,0 };
    DraggingDice = nullptr;
    DiceCount = 0;
    Price = 250;
    Money = 500;
    HP = 3;
    GameState = 0;

    HPFont = CreateFont(25, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, TEXT("맑은 고딕"));
    MoneyFont = CreateFont(30, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, TEXT("맑은 고딕"));
    TopFont = CreateFont(40, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, TEXT("맑은 고딕"));
    //g_hWnd = hWnd;

    v_Dice.assign(15, nullptr);
    bDragging = FALSE;

    srand((unsigned int)time(NULL));

    // 버튼 초기화
    ButtonInit();
    


    //IDRHandle =  CreateThread(NULL, 0, IDRTimer, NULL, 0, NULL);
    
    Proj_SemaHnd = CreateSemaphore(NULL, 1, 1, NULL);
    Monster_SemaHnd = CreateSemaphore(NULL, 1, 1, NULL);
    Money_SemaHnd = CreateSemaphore(NULL, 1, 1, NULL);
    HP_SemaHnd = CreateSemaphore(NULL, 1, 1, NULL);
    //디버그

    

}

GameHandler::~GameHandler()
{
    GameState = 0;
    Sleep(100);             // 게임이 종료될 때 여러 객체들이 소멸되는 동시에 쓰레드에서 그 객체를 참조하는듯 함.
                            // 그래서 GaemState = 0 으로 쓰레드에게 종료를 요구하고 Sleep으로 스레드 종료까지 기다림
    DeleteObject(HPFont);
    DeleteObject(MoneyFont);
    DeleteObject(TopFont);

    CloseHandle(Proj_SemaHnd);
    CloseHandle(Monster_SemaHnd);
    CloseHandle(Money_SemaHnd);
    CloseHandle(HP_SemaHnd);

    if (PlayTRHnd != NULL) CloseHandle(PlayTRHnd);
}



GameHandler* GameHandler::GetInstance()
{
    if (!Instance)
    {
        Instance = new GameHandler();
    }
    return Instance;
}
void GameHandler::DestroyInst()
{
    Clock::DestroyInst();
    if (Instance)
    {
        delete Instance;
    }
}

void GameHandler::SetHWND(HWND hWnd)
{
    this->hWnd = hWnd;
}

void GameHandler::ButtonInit()
{
    // 구매버튼
    Purchase = make_shared<ButtonObject>(POINT{ 305,450 }, 100, 100);
    Purchase->SetClickAction([this]()
        {
            if (GameState == 0) GameStart();

            if (DiceCount >= 15) return; // 주사위 자리가 없다면 리턴

            if (Money < Price) return;
            else
            {
                AddMoney(-Price);         // 돈차감
                Price += 10;
            }


            int r;
            while (1)
            {
                r = rand() % 15;
                if (v_Dice[r] == nullptr) break;
            }
            SpawnDice(r);

        });
    Purchase->SetDrawAtion([this](HDC hdc)
        {
            SetDCColor(hdc, RGB(180, 180, 180), RGB(170, 170, 170));
            Ellipse(hdc, 305, 450, 405, 550);
        });
    v_Button.push_back(Purchase);
  

    
    // 업그레이드 버튼
    int x = 145;
    int y = 592;
   
    for (int i = 0; i<5; i++)
    {
        
        UpgradeBtn[i] = make_shared<ButtonObject>(POINT{ x,y }, 70, 70);

        UpgradeBtn[i]->SetDrawAtion([=](HDC hdc)
            {
                COLORREF Color = RGB(0, 0, 0);
                switch (i)
                {
                
                case (int)DICETYPE::PURPLE: Color = RGB(108, 84, 190);
                    break;
                case (int)DICETYPE::GRAY: Color = RGB(150, 150, 150);
                    break;
                case (int)DICETYPE::ICE: Color = RGB(153, 217, 234);
                    break;
                case (int)DICETYPE::SNIPER: Color = RGB(255, 140, 55);
                    break;
                case (int)DICETYPE::BLACK: Color = RGB(0, 0, 0);
                    break;
                }

                
                // 테두리 역할 ( 선으로 하면 크기 예상이 힘듬 )
                SetDCColor(hdc, Color, NULL);
                RoundRect(hdc, x, y, x + 80, y + 80, 20, 20);

                // 주사위 안쪽 ( 흰색 )
                SetDCColor(hdc, RGB(255, 255, 255), NULL);
                RoundRect(hdc, x + DICE_BOLD, y + DICE_BOLD, x + 80 - DICE_BOLD, y + 80 - DICE_BOLD, 20, 20);


                // 폰트 변경
                HFONT Font = CreateFont(35, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, TEXT("맑은 고딕"));
                HFONT subFont = CreateFont(20, 0, 0, 0, 0, 0, 0, 0, HANGEUL_CHARSET, 0, 0, 0, VARIABLE_PITCH | FF_ROMAN, TEXT("맑은 고딕"));
                HFONT oldFont = (HFONT)SelectObject(hdc, Font);

                // 가격 표시
                WCHAR PriceText[6] = {};
                int price = 100 + GetUpgradeNum((DICETYPE)i) * 100;
                wsprintf(PriceText, TEXT("%d"), price);
                RECT PRect = { x+8,y-5,x + 70, y + 70 };
                DrawText(hdc, PriceText, -1, &PRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

                
                SelectObject(hdc, subFont);
                // 레벨 표시
                WCHAR LVText[7] = {};
                int LV = GetUpgradeNum((DICETYPE)i);
                wsprintf(LVText, TEXT("LV. %d"), LV);
                RECT LRect = { x + 8,y +20,x + 70, y + 70+20 };
                DrawText(hdc, LVText, -1, &LRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

               // 오브젝트 제거
                SelectObject(hdc, oldFont);
                DeleteObject(Font);
                DeleteObject(subFont);
                

            }
        );

        x += 85;

        UpgradeBtn[i]->SetClickAction([=]()
            {
                if (GameState == 0) return;
                int price = 100 + GetUpgradeNum((DICETYPE)i) * 100;
                cout << "가격 : " << price << endl;
                if (Money >= price)
                {
                    AddUpgradeNum((DICETYPE)i, 1);
                    AddMoney(-price);
                }
            });
        v_Button.push_back(UpgradeBtn[i]);
    }
    

}

void GameHandler::GameOver()
{
    
    int min = Clock::GetInstance()->GetTime(TIME::MINUTE);
    int sec = Clock::GetInstance()->GetTime(TIME::SECOND);

    GameState = 0;              // 각 스레드 종료 및 Projectile, Monster 객체 소멸
    
    for (int i = 0; i < 15; i++)
    {
        v_Dice[i].reset();
    }
    Clock::GetInstance()->ClockClear();

    for (int i = 0; i < 5;i++)
    {
        DIceUpgradeNum[i] = 0;
    }

    DraggingDice.reset(nullptr);
    DiceCount = 0;
    Price = 250;
    Money = 500;
    HP = 3;
    bDragging = FALSE;

    
    Clock::GetInstance()->ClockStop();

    if (hWnd != NULL)
    {
        WCHAR OverText[50];
        
        wsprintf(OverText, L"목숨이 모두 소모되었습니다.\n 결과 : %d분 %d초", min, sec);
        MessageBox(hWnd, OverText, L"게임 종료", MB_OK);
    }
}

void GameHandler::GameStart()
{

    GameState = 1;
    PlayTRHnd = CreateThread(NULL, 0, PlayTr, NULL, 0, NULL);                    // 게임의 진행관리 쓰레드
}

int GameHandler::GetGameState() const
{
    return GameState;
}
POINT GameHandler::GetMousePos() const
{
	return MousePos;
}

void GameHandler::SetMousePos(int x, int y)
{
	MousePos.x = x;
	MousePos.y = y;
}

void GameHandler::DrawGame(HDC hdc)
{

    // 간편한 색 변경을 위해 DC_PEN 사용
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(DC_PEN));                   
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(DC_BRUSH));
    HFONT oldFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);

    SetBkMode(hdc, TRANSPARENT);    // 출력되는 모든 글자의 배경이 투명해짐

    // 게임 테두리 그림자
    SetDCColor(hdc, RGB(200,200,200), NULL);
    RoundRect(hdc, 30, 20, 700, 700, 30, 30);              
    ClearDCColor(hdc);  

    // 게임 테두리
    RoundRect(hdc, 20, 0, 690, 690, 30, 30);      

    // 게임 상단바 (시간 표시 영역)
    SetDCColor(hdc, RGB(100, 100, 100),PS_NULL);;           
    RoundRect(hdc, 20, 0, 690, 70, 30,30);               
    // 게임 상단바 RoundRect의 밑부분을 흰 사각형으로 가림
    SetDCColor(hdc, RGB(255, 255, 255), NULL);
    Rectangle(hdc, 21, 56, 689, 100);     

    
    // 시간 표시
    SelectObject(hdc, TopFont);
    WCHAR TimeText[8] = { L"0 : 00" };
    int min = Clock::GetInstance()->GetTime(TIME::MINUTE);
    int sec = Clock::GetInstance()->GetTime(TIME::SECOND);

    wsprintf(TimeText, L"%d : %d%d", min, sec / 10, sec % 10);

    RECT TRect = { 325,00,430, 50 };
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, TimeText, -1, &TRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    SetTextColor(hdc, RGB(0, 0, 0));
    SelectObject(hdc, oldFont);

    // 주사위 컨테이너 그림자
    SetDCColor(hdc, RGB(240, 240, 240), NULL);
    RoundRect(hdc, 130, 170, 580, 425, 30, 30);

    

    // 주사위 컨테이너
    HPEN newPen = CreatePen(PS_SOLID, 3, RGB(200, 200, 200));    
    SelectObject(hdc, newPen);
    SelectObject(hdc, oldBrush);

    RoundRect(hdc, 135, 175, 575, 420, 30, 30);

    DeleteObject(newPen);
    

    // 몬스터 진행방향
    newPen = CreatePen(PS_SOLID, 4, RGB(200, 200, 200));
    SelectObject(hdc, newPen);

    DrawLine(hdc, 55, 120, 55, 420);
    DrawLine(hdc, 55, 120, 655, 120);
    DrawLine(hdc, 655, 120, 655, 420);

    DeleteObject(newPen);

    // 다시 DC_PEN 사용
    SelectObject(hdc, GetStockObject(DC_PEN));
    SelectObject(hdc, GetStockObject(DC_BRUSH));

    
    // 체력
    SelectObject(hdc, HPFont);
    WCHAR HPText[4] = {};
    
    for (int i = 0; i < 3; i++)
    {
        if (i <= HP - 1)
            HPText[i] = L'♥';
        else
            HPText[i] = L'♡';
    }
    
    RECT HRect = { 605,62,675, 100 };

    SetTextColor(hdc, RGB(255, 0, 0));
    DrawText(hdc, HPText, -1, &HRect, DT_SINGLELINE | DT_CENTER);
    SetTextColor(hdc, RGB(0, 0, 0));

    SelectObject(hdc, oldFont);
    

    
    // 주사위 슬롯
    SetDCColor(hdc, RGB(220, 220, 220), NULL);
    int y = 188;
    for (int i = 0; i < 3; i++)
    {
        int x = 155;
        for (int j = 0; j < 5; j++)
        {
            RoundRect(hdc, x, y, x + 70, y + 70,20,20);
            x += 83;
        }
        y += 72;
    }

    // 주사위 구매버튼 바깥 ( 버튼 객체 )
    Purchase->DrawObject(hdc);

    // 주사위 구매버튼 안쪽
    SetDCColor(hdc, RGB(220, 220, 220), RGB(210, 210, 210));
    Ellipse(hdc, 315, 460, 395, 540);

    // 구매버튼의 주사위
    SetDCColor(hdc, RGB(255, 255, 255), RGB(1, 1, 1));
    Rectangle(hdc, 340, 465, 370, 495);
    // 구매버튼 주사위 눈
    SetDCColor(hdc, RGB(0, 0, 0), NULL);
    Ellipse(hdc, 345, 470, 350, 475);
    Ellipse(hdc, 360, 470, 365, 475);
    Ellipse(hdc, 345, 485, 350, 490);
    Ellipse(hdc, 360, 485, 365, 490);
    
    // 가격/시작 표시
    SelectObject(hdc, MoneyFont);
    if (GameState == 1)
    {
        WCHAR PriceText[5] = {};
        wsprintf(PriceText, TEXT("%d"), Price);
        RECT PRect = { 320,495,390, 525 };
        DrawText(hdc, PriceText, -1, &PRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    else
    {
        WCHAR StartText[3] = {};
        wsprintf(StartText, TEXT("%s"), L"시작");
        RECT SRect = { 320,495,390, 525 };
        DrawText(hdc, StartText, -1, &SRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    

    // 돈 표시
    WCHAR MoneyText[20] = {};
    wsprintf(MoneyText, TEXT("Money : %d"), Money);
    RECT MRect = { 150,495,290, 525 };
    DrawText(hdc, MoneyText, -1, &MRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    SelectObject(hdc, oldFont);

    // 주사위 강화 컨테이너
    SetDCColor(hdc, RGB(220, 220, 220), NULL);
    RoundRect(hdc, 135, 575, 575, 680, 20, 20);

    // 주사위 강화 버튼
    for (int i = 0; i < 5; i++)
    {
        UpgradeBtn[i]->DrawObject(hdc);
    }
    
    if (GameState == 1)     // 게임 진행중에만 표시
    {

        // 몬스터 
        WaitForSingleObject(Monster_SemaHnd, INFINITE);
        auto it = l_Monster.rbegin();
        while (it != l_Monster.rend())                   // 먼저 소환한 몬스터가 겹친 몬스터중 맨앞에 위치
        {
            if ((*it)->GetState() == STATE::ALIVE)         // 살아있지 않으면
            {
                (*it)->DrawObject(hdc);
            }
            it++;
        }
         ReleaseSemaphore(Monster_SemaHnd, 1, NULL);



         // 주사위
        for (int i = 0; i < 15; i++)
        {
            if (v_Dice[i] == nullptr) continue;
            v_Dice[i]->DrawObject(hdc);
        }

        // 투사체
        WaitForSingleObject(Proj_SemaHnd, INFINITE);
        for (auto it = l_Projectile.begin(); it != l_Projectile.end(); it++)
        {
            (*it)->DrawObject(hdc);
        }
        ReleaseSemaphore(Proj_SemaHnd, 1, NULL);


        // 드래그중인 임시 주사위
        if (IsDragging() == TRUE)
        {
            DraggingDice->DrawObject(hdc);
        }
    }
    

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

}

void GameHandler::DrawLine(HDC hdc, int x, int y, int x2, int y2)
{
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x2, y2);
}

void GameHandler::SetDCColor(HDC hdc, COLORREF B_Color, COLORREF P_Color)
{

    SetDCBrushColor(hdc, B_Color);

    if (P_Color == NULL) SetDCPenColor(hdc, B_Color);    
    else SetDCPenColor(hdc, P_Color);
}

void GameHandler::ClearDCColor(HDC hdc)
{
    SetDCBrushColor(hdc, RGB(255, 255, 255));
    SetDCPenColor(hdc, RGB(0, 0, 0));
}

void GameHandler::OnMouseClicked(int x, int y)                  // WM_LBUTTONDOWN 에서 호출
{
    
    for (int i = 0; i < (int)v_Button.size(); i++)   
    {
        if (v_Button[i]->IsOverlappedPoint(x, y))                // 클릭한 위치가 버튼의 범위 내에 있을경우 참
        {
            v_Button[i]->OnClickedObject();                      // 버튼에 등록한 함수를 실행
        }
    }
    for (int i = 0; i < 15; i++)
    {
        if (v_Dice[i] == nullptr) continue;
        if (v_Dice[i]->IsOverlappedPoint({ x,y }))              // 클릭한 위치가 주사위의 범위 내에 있을경우 참
        {
            if (bDragging == FALSE)                             // 현재 드래그중이 아니라면
            {
                DraggedDice = v_Dice[i].get();                  // 원본 객체 주소 저장, 원본객체는 가만히 있음

                DraggingDice = make_unique<DiceBase>(*DraggedDice);     // 스마트 포인터 이용하여 클릭한 다이스를 복사 생성 / 마우스에 드래깅되는 객체
                DraggingDice->MoveToMouse(GetMousePos());

                DraggedDice->SetSelected(TRUE);
                //DraggedDice->ReDraw();                            
                bDragging = TRUE;

                break;
            }
        }
    }
}

// WM_MOUSEMOVE 에서 호출
void GameHandler::OnMouseMoved()
{
    if (DraggingDice == nullptr) return;
    DraggingDice->MoveToMouse(GetMousePos());
    //InvalidateRect(g_hWnd, NULL, FALSE);
}

// WM_LBUTTONUP 에서 호출
void GameHandler::OnMouseReleased(int x, int y)             
{
    if (IsDragging() == TRUE)                               // 드래그중이였다면
    {


        // 다이스끼리의 겹침과 상관없이 실행되는 부분. 드래그를 취소함

        bDragging = FALSE;                                                  // 드래깅 끝냄
        DraggingDice.reset(nullptr);                                        // 마우스를 따라다니는 임시 객체를 delete하고 변수를 nullptr로 변경

        DraggedDice->SetSelected(FALSE);                                    // 색상 돌려놓음
        

        if (DraggedDice->GetEye() < 6)                                      // 눈이 6이하의 주사위 일때
        {

            for (int i = 0; i < 15; i++)                                    // 모든 주사위 탐색
            {

                if (v_Dice[i] == nullptr) continue;
                if (v_Dice[i]->IsOverlappedPoint({ x,y }))                  // 마우스를 떼었던 위치가 주사위와 겹친다면
                {
                    if (*v_Dice[i] == *DraggedDice)                         // 타입과 주사위 눈이 서로 같을 경우
                    {
                        if (v_Dice[i].get() == DraggedDice) continue;       // 자기 자신은 예외
                        v_Dice[i]->AddEye(1);                               // 눈을 증가시킴

                        int temp = DraggedDice->GetSlot();
                        v_Dice[temp]->StopTr();
                        v_Dice[temp].reset();
                        DraggedDice = nullptr;                              // 드래그중이던 주사위 제거
                        DiceCount--;
                        break;
                    }
                }
            }
        }
        

        DraggedDice = nullptr;                              

        //InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

BOOL GameHandler::IsDragging() const
{
    return bDragging;
   
}

void GameHandler::AddMoney(int newMoney)
{
    WaitForSingleObject(Money_SemaHnd, INFINITE);
    Money += newMoney; 
    cout << "[MONEY]\t"<<Money << endl;
    ReleaseSemaphore(Money_SemaHnd, 1, NULL);
}

void GameHandler::AddHP(int newHP)
{
    WaitForSingleObject(HP_SemaHnd, INFINITE);
    HP += newHP;
    cout << "[HP]\t"<< HP << endl;
    ReleaseSemaphore(HP_SemaHnd, 1, NULL);
    if (HP <= 0) GameOver();
}

void GameHandler::AddUpgradeNum(DICETYPE type, int num)
{
    DIceUpgradeNum[(int)type]++;
}

int GameHandler::GetUpgradeNum(DICETYPE type)
{

    return DIceUpgradeNum[(int)type];
}

// MonsterTr 종료전 Monster객체를 소멸시키고 리스트에서 제거하기 위한 함수
void GameHandler::DeleteMonster(MonsterBase *Monster)            
{
    WaitForSingleObject(Monster_SemaHnd, INFINITE);
    for (auto it = l_Monster.begin(); it != l_Monster.end(); it++)
    {
        if (it->get() == Monster)                               
        {
            while (it->use_count() > 2)                         // l_Monster와 MonsterTr 이외에도 객체를 사용하고있다면 대기
            {     
                Sleep(10);                                     // 0.01초에 한번씩 체크
            }
            
            it = l_Monster.erase(it);
            
            break;
        }
    }
    ReleaseSemaphore(Monster_SemaHnd, 1, NULL);
    
}

//ProjectileTr 종료전 Projectile객체를 소멸시키고 리스트에서 제거하기 위한 함수
void GameHandler::DeleteProjectile(ProjectileBase* Projectile)             
{
    WaitForSingleObject(Proj_SemaHnd, INFINITE);
    for (auto it = l_Projectile.begin(); it != l_Projectile.end(); it++)
    {
        if (it->get() == Projectile)                           
        {
            it = l_Projectile.erase(it);
            break;
        }
    }
    ReleaseSemaphore(Proj_SemaHnd, 1, NULL);

}


void GameHandler::AddProjectile(shared_ptr<ProjectileBase> Proj)
{
    WaitForSingleObject(Proj_SemaHnd, INFINITE);
    l_Projectile.push_back(Proj);
    ReleaseSemaphore(Proj_SemaHnd, 1, NULL);   
}

void GameHandler::SpawnMonster(MONSTER Type, int HP)
{
    WaitForSingleObject(Monster_SemaHnd, INFINITE);
    l_Monster.push_back(make_shared<MonsterBase>(HP));
    CreateThread(NULL, 0, MonsterTr, &l_Monster.back(), 0, NULL);
    ReleaseSemaphore(Monster_SemaHnd, 1, NULL);
}

void GameHandler::SpawnDice(int slot)
{
    shared_ptr<DiceBase> newDice = NULL;
    switch (rand()%5)
    {
    case 0:
        newDice = make_shared<DiceBase>(slot, 1);
        break;
    case 1:
        newDice = make_shared<PurpleDIce>(slot, 1);
        break;
    case 2:
        newDice = make_shared<GrayDice>(slot, 1);
        break;
    case 3:
        newDice = make_shared<IceDice>(slot, 1);
        break;
    case 4:
        newDice = make_shared<SniperDice>(slot, 1);
        break;
    default:
        newDice = make_shared<DiceBase>(slot, 1);
        break;
    }
    v_Dice[slot] = newDice;
    HANDLE hnd = CreateThread(NULL, 0, DiceTr, &v_Dice[slot], 0, NULL);
    DiceCount++;
}

shared_ptr<MonsterBase> GameHandler::GetMonsterRef(ATKTYPE Type) const
{

    shared_ptr<MonsterBase> s = NULL;
    WaitForSingleObject(Monster_SemaHnd, INFINITE);
    if (!l_Monster.empty())
    {
        switch (Type)
        {
        case ATKTYPE::FRONT:
            s = l_Monster.front();
            break;
        case ATKTYPE::BACK:
            s = l_Monster.back();
            break;
        case ATKTYPE::RANDOM:
        {
            int Target = rand() % l_Monster.size();
            int index = 0;
            for (auto it = l_Monster.begin(); it != l_Monster.end(); it++)
            {
                if (Target == index++) s = *it;
            }
        }
            break;
        default:
            break;
        
        }
    }

    ReleaseSemaphore(Monster_SemaHnd, 1, NULL);
    return s;
}



// 몬스터마다 스레드 부여
DWORD WINAPI MonsterTr(LPVOID Param)
{
   shared_ptr<MonsterBase> Monster = *(shared_ptr<MonsterBase>*)Param;

   GameHandler* GHnd = GameHandler::GetInstance();
   BOOL bArrival = FALSE;
   while (Monster->GetState() == STATE::ALIVE)
   {
       if (GHnd->GetGameState() == 0)
       {
           bArrival = TRUE;
           break;
       }
        BOOL bMoveEnd = Monster->MoveNextPoint();   // 몬스터 이동. 만약 끝 지점에 도착하면 TRUE 반환
        
        if (bMoveEnd)
        {
            Monster->SetState(STATE::ARRIVAL);
            bArrival = TRUE;
            GHnd->AddHP(-1);
            break;
        }

       Sleep(Monster->GetSleepTime());
   }   
   if (bArrival == FALSE) GHnd->AddMoney(50);
   GHnd->DeleteMonster(Monster.get());
   
   return 0;
}

// 투사체 움직임
DWORD WINAPI ProjectileTr(LPVOID Param)
{
    shared_ptr<ProjectileBase> Projectile = ((pair<shared_ptr<ProjectileBase>, shared_ptr<MonsterBase>>*)Param)->first;
    shared_ptr<MonsterBase> Target = ((pair<shared_ptr<ProjectileBase>, shared_ptr<MonsterBase>>*)Param)->second;
    
    GameHandler* GHnd = GameHandler::GetInstance();
    BOOL result = FALSE;;
    while (Target->GetState() == STATE::ALIVE && GHnd->GetGameState() != 0) 
    {

        result = Projectile->MoveToTarget(Target.get()); // 이동 후  타겟과 겹치면 TRUE 반환
        if (result)
        {
            Target->TakeDamage(Projectile->GetPower());
            Target->SetDebuff(Projectile->GetDebuff());
            break;
        }
        else
        Sleep(17);
    }
    Target = NULL;
    if(result == FALSE) Projectile->Disappear();        // 천천히 사라지는 효과
    
    GHnd->DeleteProjectile(Projectile.get());
    return 0;
}

DWORD WINAPI DiceTr(LPVOID Param)
{
    shared_ptr<DiceBase> Dice = *(shared_ptr<DiceBase>*)Param;
    GameHandler* GHnd = GameHandler::GetInstance();

    while (!Dice->IsReadyToDel() && GHnd->GetGameState() != 0)                                               // 스레드 종료를 위한 플래그 변수
    {
        shared_ptr<MonsterBase> Target;
        ATKTYPE AttackType = Dice->GetAttackType();
        

        Target = GHnd->GetMonsterRef(AttackType);
        if (Target != NULL)
        {
            int UpgradNum = GHnd->GetUpgradeNum(Dice->GetType());

            shared_ptr<ProjectileBase> Proj = Dice->SpawnProj(UpgradNum);
            GHnd->AddProjectile(Proj);                                      // 생성한 Projectile을 리스트에 등록. 동기화 되어있음
            
            pair<shared_ptr<ProjectileBase>, shared_ptr<MonsterBase>> rParam;
            rParam = make_pair(Proj, Target);

            CreateThread(NULL, 0, ProjectileTr, &rParam, 0, NULL);
        }
        Target = NULL;
        Sleep(DWORD(Dice->GetSpeed() * 1000));
    }
    cout << "종료";
    return 0;
}

DWORD WINAPI PlayTr(LPVOID Param)
{
    GameHandler* GHnd = GameHandler::GetInstance();
    Clock* clock = Clock::GetInstance();
    clock->ClockStart();

    int HPRatio = 0;
    int Time = 0;
    while (GHnd->GetGameState() != 0)   // 패배 전까지 반복
    {
        Time = clock->GetTime(TIME::MINUTE) * 60 + clock->GetTime(TIME::SECOND);
        
        GHnd->SpawnMonster(MONSTER::ORIGINAL, 30 + 10 * (Time / 8));

        int SleepTime = max(700, 2000 - Time*3);
        Sleep(SleepTime);
    }
    return 0;
}

