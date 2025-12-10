#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;  // Константа "бесконечности" для алгоритма минимакс

// Класс Logic содержит всю игровую логику: поиск ходов, оценку позиции, алгоритм минимакс
class Logic
{
public:
    // Конструктор: инициализирует логику с доской и конфигурацией
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        // Инициализируем генератор случайных чисел: либо со случайным сидом, либо с фиксированным (0)
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");  // Режим оценки позиции
        optimization = (*config)("Bot", "Optimization");    // Уровень оптимизации алгоритма
    }

    // Находит лучшие ходы для бота с использованием алгоритма минимакс
    // color: цвет бота (false - белые, true - черные)
    // Возвращает вектор ходов, которые должен сделать бот (может быть несколько, если есть серия ударов)
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем вспомогательные структуры для хранения лучших ходов и состояний
        next_best_state.clear();
        next_move.clear();

        // Запускаем поиск лучшего хода с начального состояния
        // Параметры: текущее состояние доски, цвет бота, координаты (-1, -1) - нет активной шашки,
        // состояние 0 (начальное), альфа = -1 (начальное значение для максимизирующего игрока)
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Восстанавливаем цепочку лучших ходов из вспомогательных структур
        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            // Добавляем лучший ход из текущего состояния
            res.push_back(next_move[cur_state]);
            // Переходим к следующему состоянию (для серии ударов)
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1); // Пока есть следующие ходы

        return res;
    }

private:
    // Выполняет ход на переданной матрице доски и возвращает новое состояние
    // mtx: текущее состояние доски
    // turn: ход для выполнения
    // Возвращает новое состояние доски после хода
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если ход включает бой, удаляем побитую шашку
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;

        // Проверяем превращение в дамку
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;

        // Перемещаем шашку
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Оценивает позицию на доске с точки зрения указанного игрока
    // mtx: состояние доски для оценки
    // first_bot_color: цвет бота, для которого вычисляется оценка (false - белые, true - черные)
    // Возвращает числовую оценку позиции (чем выше, тем лучше для first_bot_color)
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    // Дополнительная оценка: шашки ближе к дамочному полю получают бонус
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);  // Белые шашки: чем ближе к верху (превращение), тем лучше
                    b += 0.05 * (mtx[i][j] == 2) * (i);      // Черные шашки: чем ближе к низу, тем лучше
                }
            }
        }
        // Если бот играет черными, меняем местами оценки
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Если у противника не осталось шашек - максимальная оценка (победа)
        if (w + wq == 0)
            return INF;
        // Если у бота не осталось шашек - минимальная оценка (поражение)
        if (b + bq == 0)
            return 0;

        // Коэффициент ценности дамки (обычно дамка ценнее обычной шашки)
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Формула оценки: (шашки бота + дамки бота * коэффициент) / (шашки противника + дамки противника * коэффициент)
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Находит лучший первый ход для текущей позиции (входная точка алгоритма минимакс)
    // mtx: текущее состояние доски
    // color: цвет бота (для которого ищем лучший ход)
    // x, y: координаты шашки, которая должна продолжить ход (если есть серия ударов)
    // state: текущий индекс состояния в вспомогательных массивах
    // alpha: лучшая оценка для максимизирующего игрока (для альфа-бета отсечения)
    // Возвращает оценку лучшего хода
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        // Добавляем новое состояние в вспомогательные массивы
        next_best_state.push_back(-1); // Пока не знаем следующее лучшее состояние
        next_move.emplace_back(-1, -1, -1, -1); // Пока не знаем лучший ход

        double best_score = -1; // Лучшая оценка для текущего состояния

        // Если это не начальное состояние (state != 0), ищем ходы для конкретной шашки
        // (это нужно для продолжения серии ударов)
        if (state != 0)
            find_turns(x, y, mtx);

        auto turns_now = turns; // Копируем найденные ходы
        bool have_beats_now = have_beats; // Запоминаем, есть ли среди них взятия

        // Если нет обязательных ударов и это не начальное состояние,
        // переходим к рекурсивному поиску для следующего игрока
        if (!have_beats_now && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Перебираем все возможные ходы в текущем состоянии
        for (auto turn : turns_now)
        {
            size_t next_state = next_move.size(); // Индекс следующего состояния

            double score;
            if (have_beats_now)
            {
                // Если есть взятия, продолжаем серию ударов (ходит тот же игрок)
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else
            {
                // Если нет взятий, переходим к следующему игроку
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Если нашли ход с лучшей оценкой, обновляем лучший ход и оценку
            if (score > best_score)
            {
                best_score = score;
                // Запоминаем следующее состояние (если есть серия ударов) или -1
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                // Запоминаем лучший ход для текущего состояния
                next_move[state] = turn;
            }
        }

        return best_score;
    }

    // Рекурсивная функция алгоритма минимакс с альфа-бета отсечением
    // mtx: текущее состояние доски
    // color: цвет текущего игрока (false - белые, true - черные)
    // depth: текущая глубина рекурсии (0 - начало)
    // alpha: лучшая оценка для максимизирующего игрока (начальное значение -1)
    // beta: лучшая оценка для минимизирующего игрока (начальное значение INF+1)
    // x, y: координаты шашки, которая должна продолжить ход (для серии ударов)
    // Возвращает оценку позиции для текущего игрока
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Базовый случай рекурсии: достигнута максимальная глубина поиска
        if (depth == Max_depth)
        {
            // Оцениваем позицию с точки зрения игрока, который должен был ходить на этой глубине
            return calc_score(mtx, (depth % 2 == color));
        }

        // Если заданы координаты шашки, ищем ходы только для нее (продолжение серии ударов)
        if (x != -1)
        {
            find_turns(x, y, mtx);
        }
        else // Иначе ищем все ходы для текущего цвета
        {
            find_turns(color, mtx);
        }

        auto turns_now = turns; // Копируем найденные ходы
        bool have_beats_now = have_beats; // Запоминаем, есть ли взятия

        // Если нет обязательных ударов и была задана конкретная шашка,
        // значит серия ударов закончилась - переходим к ходу следующего игрока
        if (!have_beats_now && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если нет доступных ходов - терминальное состояние игры
        if (turns.empty())
        {
            // Если на глубине depth ходит текущий игрок (depth % 2 == 0 для максимизирующего),
            // то у него нет ходов - это проигрышная позиция (оценка 0 для минимизирующего, INF для максимизирующего)
            return (depth % 2 ? 0 : INF);
        }

        // Инициализируем минимальную и максимальную оценки
        double min_score = INF + 1; // Для минимизирующего игрока (четная глубина)
        double max_score = -1;      // Для максимизирующего игрока (нечетная глубина)

        // Перебираем все возможные ходы
        for (auto turn : turns_now)
        {
            double score = 0.0;

            if (!have_beats_now && x == -1)
            {
                // Обычный ход (без серии ударов): рекурсивно оцениваем следующее состояние
                // Ход делает текущий игрок, затем ход переходит к противнику
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else
            {
                // Продолжение серии ударов: та же шашка должна бить дальше
                // Ход остается у текущего игрока (глубина не увеличивается)
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }

            // Обновляем минимальную и максимальную оценки
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Альфа-бета отсечение для оптимизации
            if (depth % 2) // Если глубина нечетная - ход максимизирующего игрока
            {
                alpha = max(alpha, max_score); // Улучшаем нижнюю границу (альфа)
            }
            else // Если глубина четная - ход минимизирующего игрока
            {
                beta = min(beta, min_score); // Улучшаем верхнюю границу (бета)
            }

            // Если достигнуто условие для отсечения (альфа >= бета),
            // дальнейший поиск в этой ветке не улучшит результат
            if (optimization != "O0" && alpha >= beta)
            {
                // Возвращаем оценку с небольшим смещением, чтобы сохранить порядок ходов
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }

        // Возвращаем оценку в зависимости от того, чей сейчас ход
        // На четной глубине ходит минимизирующий игрок (возвращаем минимальную оценку)
        // На нечетной глубине ходит максимизирующий игрок (возвращаем максимальную оценку)
        return (depth % 2 ? max_score : min_score);
    }

public:
    // === Пункт 16: Комментарии к перегруженным функциям find_turns ===

    // Находит все возможные ходы для указанного цвета на текущей доске
    // color: цвет игрока, для которого ищем ходы (false - белые, true - черные)
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Находит все возможные ходы для указанной шашки на текущей доске
    // x, y: координаты шашки
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Находит все возможные ходы для указанного цвета на переданной матрице доски
    // Результат сохраняется в членах класса turns и have_beats
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Находит все возможные ходы для указанной шашки на переданной матрице доски
    // x, y: координаты шашки
    // mtx: матрица доски для анализа
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];

        // Сначала проверяем возможные взятия (бои)
        switch (type)
        {
        case 1:  // Белая шашка
        case 2:  // Черная шашка
            // Проверка взятий для обычных шашек
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    // Условия для взятия: конечная клетка пуста, между ними шашка противника
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:  // Дамки (3 - белая, 4 - черная)
            // Проверка взятий для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    // Дамка может бить через несколько клеток
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            // Если встретили свою шашку или вторую шашку противника - прерываем
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }

        // Если найдены взятия, возвращаем только их (по правилам шашек, если есть бой - нужно бить)
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }

        // Если взятий нет, ищем обычные ходы
        switch (type)
        {
        case 1:  // Белая шашка (ходит вверх)
        case 2:  // Черная шашка (ходит вниз)
            // Проверка обычных ходов для шашек
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);  // Белые ходят вверх (уменьшение i), черные - вниз
            for (POS_T j = y - 1; j <= y + 1; j += 2)
            {
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                    continue;
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default:  // Дамки
            // Проверка обычных ходов для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    // === Пункт 18: Комментарии к полям класса ===

    vector<move_pos> turns;  // Список найденных возможных ходов
    bool have_beats;         // Флаг, указывающий, есть ли среди ходов взятия (бои)
    int Max_depth;           // Максимальная глубина поиска для алгоритма минимакс

private:
    // Приватные поля класса:
    default_random_engine rand_eng;  // Генератор случайных чисел для перемешивания ходов
    string scoring_mode;             // Режим оценки позиции ("NumberAndPotential" или другой)
    string optimization;             // Уровень оптимизации алгоритма ("O0", "O1", и т.д.)
    vector<move_pos> next_move;      // Вспомогательный массив для хранения лучших ходов в алгоритме минимакс
    vector<int> next_best_state;     // Вспомогательный массив для связи состояний в алгоритме минимакс
    Board* board;                    // Указатель на объект доски
    Config* config;                  // Указатель на объект конфигурации
};