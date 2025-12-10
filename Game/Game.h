#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Основной игровой цикл - запускает и управляет игрой в шашки
    int play()
    {
        // Засекаем время начала игры для записи в лог
        auto start = chrono::steady_clock::now();

        // Если включен режим повтора игры, перезагружаем логику и конфигурацию
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            // Иначе начинаем новую игру с начальной отрисовкой доски
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;           // Номер текущего хода (начинаем с -1, чтобы первый ход был 0)
        bool is_quit = false;        // Флаг выхода из игры

        // Максимальное количество ходов до ничьей (правило 50 ходов)
        const int Max_turns = config("Game", "MaxNumTurns");

        // Главный игровой цикл: продолжается пока не достигнут максимальный номер хода
        while (++turn_num < Max_turns)
        {
            beat_series = 0;  // Сбрасываем счетчик серии ударов (для шашки, которая бьет несколько раз подряд)

            // Находим все возможные ходы для текущего игрока (0 - белые, 1 - черные)
            logic.find_turns(turn_num % 2);

            // Если ходов нет - игра окончена (у текущего игрока нет допустимых ходов)
            if (logic.turns.empty())
                break;

            // Устанавливаем глубину поиска для алгоритма минимакс в зависимости от уровня сложности бота
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Проверяем, является ли текущий игрок человеком (а не ботом)
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Ход человека: получаем ответ от игрока
                auto resp = player_turn(turn_num % 2);

                // Обработка различных ответов игрока
                if (resp == Response::QUIT)
                {
                    is_quit = true;    // Игрок решил выйти
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;  // Игрок хочет переиграть
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // Обработка отмены хода (возврата на ход назад)
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        // Если предыдущий ход был сделан ботом и не было серии ударов, отменяем два хода
                        board.rollback();
                        --turn_num;
                    }
                    // Отменяем последний ход
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                // Ход бота
                bot_turn(turn_num % 2);
        }

        // Засекаем время окончания игры и записываем длительность в лог-файл
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если был запрос на повтор игры, запускаем play() рекурсивно
        if (is_replay)
            return play();
        // Если игрок вышел, возвращаем 0
        if (is_quit)
            return 0;

        // Определяем результат игры:
        int res = 2;  // По умолчанию - победа черных (1), но изменим ниже
        if (turn_num == Max_turns)
        {
            res = 0;  // Ничья (достигнут максимальный номер хода)
        }
        else if (turn_num % 2)
        {
            res = 1;  // Победа черных (последний ход был за черными, значит белые не могут ходить)
        }

        // Показываем финальный экран с результатом игры
        board.show_final(res);

        // Ожидаем реакции игрока на финальном экране
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();  // Запускаем игру заново
        }

        // Возвращаем результат игры: 0 - ничья, 1 - победа черных, 2 - победа белых
        return res;
    }

  private:
      // Обрабатывает ход бота (искусственного интеллекта)
  // Параметр color: цвет бота (false - белые, true - черные)
      void bot_turn(const bool color)
      {
          // Засекаем время начала хода для записи в лог
          auto start = chrono::steady_clock::now();

          // Получаем задержку хода бота из конфигурации (для имитации "размышления")
          auto delay_ms = config("Bot", "BotDelayMS");

          // Создаем отдельный поток для задержки, чтобы расчет хода и задержка выполнялись параллельно
          thread th(SDL_Delay, delay_ms);

          // Находим наилучшие ходы для бота с использованием алгоритма минимакс
          auto turns = logic.find_best_turns(color);

          // Дожидаемся завершения потока с задержкой
          th.join();

          bool is_first = true;

          // Выполняем найденные ходы (может быть несколько, если есть серия ударов)
          for (auto turn : turns)
          {
              // Для второго и последующих ходов в серии также добавляем задержку
              if (!is_first)
              {
                  SDL_Delay(delay_ms);
              }
              is_first = false;

              // Увеличиваем счетчик серии ударов, если ход является боем
              beat_series += (turn.xb != -1);

              // Выполняем перемещение шашки на доске
              board.move_piece(turn, beat_series);
          }

          // Засекаем время окончания хода и записываем длительность в лог-файл
          auto end = chrono::steady_clock::now();
          ofstream fout(project_path + "log.txt", ios_base::app);
          fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
          fout.close();
      }
    

    // Обрабатывает ход игрока-человека
// Параметр color: цвет игрока (false - белые, true - черные)
// Возвращает Response - результат действия игрока
    Response player_turn(const bool color)
    {
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;

        // Собираем все начальные позиции, с которых можно сделать ход
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }

        // Подсвечиваем на доске клетки, с которых можно начать ход
        board.highlight_cells(cells);

        move_pos pos = { -1, -1, -1, -1 };  // Структура для хранения выбранного хода
        POS_T x = -1, y = -1;             // Координаты выбранной шашки

        // Цикл выбора первой части хода (выбор шашки для перемещения)
        while (true)
        {
            // Ожидаем ответ от пользователя (клик мыши)
            auto resp = hand.get_cell();

            // Если ответ не выбор клетки, возвращаем его (QUIT, REPLAY и т.д.)
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            // Получаем координаты выбранной клетки
            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

            bool is_correct = false;

            // Проверяем, является ли выбранная клетка допустимой для начала хода
            for (auto turn : logic.turns)
            {
                // Если клетка совпадает с начальной позицией какого-либо хода
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Если выбран полный ход (от одной клетки к другой) - завершаем выбор
                if (turn == move_pos{ x, y, cell.first, cell.second })
                {
                    pos = turn;
                    break;
                }
            }

            // Если найден полный ход, выходим из цикла
            if (pos.x != -1)
                break;

            // Если выбрана некорректная клетка, сбрасываем выбор
            if (!is_correct)
            {
                if (x != -1)
                {
                    // Сбрасываем подсветку и выделение
                    board.clear_active();
                    board.clear_highlight();
                    // Восстанавливаем подсветку допустимых начальных клеток
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }

            // Запоминаем выбранную шашку
            x = cell.first;
            y = cell.second;

            // Обновляем отображение: очищаем подсветку, выделяем выбранную шашку
            board.clear_highlight();
            board.set_active(x, y);

            // Подсвечиваем клетки, куда можно походить выбранной шашкой
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }

        // Очищаем подсветку и выделение после завершения выбора
        board.clear_highlight();
        board.clear_active();

        // Выполняем перемещение шашки
        board.move_piece(pos, pos.xb != -1);

        // Если это был простой ход (без боя), возвращаем OK
        if (pos.xb == -1)
            return Response::OK;

        // Если был бой, продолжаем серию ударов
        beat_series = 1;

        // Цикл продолжения серии ударов (когда шашка может бить несколько раз подряд)
        while (true)
        {
            // Ищем возможные продолжения боя для шашки, которая только что побила
            logic.find_turns(pos.x2, pos.y2);

            // Если нет возможных ударов, завершаем серию
            if (!logic.have_beats)
                break;

            // Подсвечиваем клетки, куда можно бить
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);

            // Цикл выбора продолжения удара
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

                bool is_correct = false;

                // Проверяем, является ли выбранная клетка допустимой для продолжения боя
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;  // Запоминаем выбранный ход
                        break;
                    }
                }

                // Если выбрана некорректная клетка, продолжаем ожидание выбора
                if (!is_correct)
                    continue;

                // Очищаем подсветку, выполняем ход и увеличиваем счетчик серии ударов
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK;
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
