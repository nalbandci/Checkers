#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand обрабатывает пользовательский ввод (мышь, клавиатура, события окна)
// и преобразует его в игровые команды (Response)
class Hand
{
public:
    // Конструктор: принимает указатель на доску для получения информации о размерах и состоянии
    Hand(Board* board) : board(board)
    {
    }

    // Функция get_cell() ожидает действия пользователя и возвращает выбранную клетку доски
    // Возвращает tuple: (тип ответа, координата X клетки, координата Y клетки)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;  // Структура для хранения события SDL
        Response resp = Response::OK;  // Изначально предполагаем успешный ответ
        int x = -1, y = -1;     // Абсолютные координаты мыши на экране
        int xc = -1, yc = -1;   // Координаты клетки на доске (0-7)

        // Бесконечный цикл ожидания события
        while (true)
        {
            // Проверяем наличие событий в очереди SDL
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Событие закрытия окна (крестик в углу)
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN:  // Событие нажатия кнопки мыши
                    x = windowEvent.motion.x;  // Получаем координаты клика
                    y = windowEvent.motion.y;

                    // Преобразуем абсолютные координаты в координаты клетки доски
                    // Доска имеет размер 10x10 клеток (8 игровых + 2 служебных)
                    xc = int(y / (board->H / 10) - 1);  // Вычисляем строку (0-7)
                    yc = int(x / (board->W / 10) - 1);  // Вычисляем столбец (0-7)

                    // Проверка клика на кнопке "Назад" (левый верхний угол, клетка (-1, -1))
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;  // Кнопка "Назад" активна, если есть история ходов
                    }
                    // Проверка клика на кнопке "Повтор игры" (правая верхняя область)
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Проверка, что клик в пределах игрового поля (8x8)
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;  // Выбрана клетка на доске
                    }
                    else
                    {
                        // Клик вне игрового поля и не на кнопках - игнорируем
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:  // События, связанные с окном
                    // Обработка изменения размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();  // Пересчитываем размеры элементов доски
                        break;
                    }
                }

                // Если получен значимый ответ (не Response::OK), выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }

        // Возвращаем результат: тип ответа и координаты клетки (или -1, -1 для кнопок)
        return { resp, xc, yc };
    }

    // Функция wait() ожидает любого действия пользователя на финальном экране
    // Используется после окончания игры для ожидания решения игрока
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    // При изменении размера окна пересчитываем размеры доски
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    // Обрабатываем клик мыши на финальном экране
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;

                    // Преобразуем координаты аналогично get_cell()
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // Проверяем, была ли нажата кнопка "Повтор игры"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }

                // Если получен ответ (QUIT или REPLAY), выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

private:
    Board* board;  // Указатель на объект доски для доступа к размерам и состоянию
};