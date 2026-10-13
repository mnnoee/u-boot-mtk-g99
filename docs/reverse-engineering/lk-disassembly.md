# Дизассемблирование стокового LK / Disassembling the stock LK

**Источник:** `lk_a` / `lk_b` — стоковый MediaTek Little Kernel, которым
прошит телефон изначально.

**Source:** `lk_a` / `lk_b` — the stock MediaTek Little Kernel the phone ships with.

---

## Зачем / Why

Спецификаций на дисплей и SD-контроллер MT6789 нет. Но LK делает ровно то,
что нужно, и делает это **правильно** — телефон с ним работает. Значит, порядок
операций и значения регистров можно взять из него.

There are no specifications for the MT6789 display or SD controller. But the LK
does exactly the right thing, and the phone works with it. Therefore the sequence
of operations and the register values can be taken from it.

---

## Как читалось / How it was read

| Шаг / Step | Что сделано / What was done |
|---|---|
| 1 | LK выгружен из раздела, разобран `liblk` / dumped from the partition, parsed with `liblk` |
| 2 | Импортирован в Ghidra как ARM64 / imported into Ghidra as ARM64 |
| 3 | Получен декомпилированный исходник — `lk_a_lk.bin.c`, **~98 000 строк** / decompiled source, **~98 000 lines** |
| 4 | Функции искались по константам и по обращениям к регистрам / functions located via constants and register accesses |
| 5 | Найденные последовательности сверены с живым Android / findings cross-checked against live Android |

### Как искать нужную функцию / How to find the right function

Константы — надёжный способ, потому что имен в бинарнике нет:

Constants are the reliable way, because the binary has no names:

| Что искали / Looked for | Константа / Marker |
|---|---|
| Инициализация дисплея / display init | базы `0x14005000`, `0x14013000`, регистры OVL0 |
| Отправка DSI-команд / DSI command send | коды `0x2C` (Write Memory Start), шаблон `0x2c3909` |
| Слои OVL / OVL layers | `FUN_00041b88` — конфигурация слоёв, vtable |
| Конвейер OVL / OVL pipeline | `FUN_0003c8b8` — отправка через CMDQ |
| Инициализация MSDC1 / MSDC1 init | регистры `0x11240000`, `EMMC_CFG0 = 0x70` |
| Питание SD / SD power | pwrap, `0x10006000` |

> **Имя функции в Ghidra (`FUN_0003c8b8`) — не описание.**
> Повествование восстановлено по тому, какие регистры функция пишет
> и в каком порядке.
>
> **A Ghidra name is not a description.** Behaviour was reconstructed from which
> registers the function writes, and in what order.

---

## Что в итоге нашлось / What came out of it

### Дисплей / Display

Получена последовательность инициализации OVL0, которую затем повторил наш
драйвер. Отсюда взяты:

The OVL0 init sequence was obtained and then reproduced by our driver. From it:

- состав слоёв и значение `SRC_CON = 0x3` (слои 0 и 1 остаются активны);
  the layer composition and `SRC_CON = 0x3` (layers 0 and 1 stay active);
- формат `L0_CON = 0x000001ff` (ARGB8888, 1080×2400);
  the format `L0_CON = 0x000001ff` (ARGB8888, 1080×2400);
- **смещение FIFO команд DSI: `0xd00`** — не `0x200`, как в старых чипах;
  **DSI command FIFO offset: `0xd00`** — not `0x200` as on older chips;
- код DSI-команды записи кадра: `0x2c3909`.
  the frame write DSI command code: `0x2c3909`.

Подробнее / details: [`../hardware/display.md`](../hardware/display.md)

### SD-контроллер (MSDC1) / SD controller

Разобрана функция инициализации, которая в LK выполнялась «по-русски»:

The init function the LK performed "in Russian" was decoded:

```text
SDC_ARG   = 0
SDC_CMD   = 0x1000
EMMC_CFG0 (0x70) |= 0x2000, |= 2, ждём EMMC_STS (0x78) бит 4, затем &= ~0x8000
DAT_RDDLY0..3 (0xf8..0x104) = 0
```

Это **eMMC-boot handshake**. В v219 мы его отключили — подозревая, что он
удерживает CMD/DAT прижатыми в режиме SD. Не помогло, но гипотеза проверена.

This is an **eMMC-boot handshake**. In v219 we disabled it, suspecting it holds
CMD/DAT low in SD mode. It did not help, but the hypothesis was tested.

> ⚠️ **Ловушка:** этот кусок LK относится к **eMMC**, а не к SD.
> Переносить его 1:1 на SD-карту — ошибка, и именно её проверял v219.
>
> ⚠️ **Trap:** this LK fragment belongs to **eMMC**, not SD. Porting it 1:1 to an
> SD card is a mistake — and exactly that is what v219 tested.

---

## Проверка результатов / Verifying the results

Ценность дизассемблирования — не в тексте, а в **сверке**:

The value of disassembly is not the text but the **cross-check**:

| Что сверено / Checked | Итог / Outcome |
|---|---|
| Значения OVL0 из LK vs живые регистры Android / LK OVL0 values vs live Android registers | совпали после v187 / matched after v187 |
| Смещение DSI CMDQ `0xd00` vs поведение железа / DSI CMDQ offset vs hardware behaviour | подтверждено / confirmed |
| Последовательность MSDC1 из LK vs работа Android / LK MSDC1 sequence vs working Android | расхождения найдены, часть — ложные следы / divergences found, some are red herrings |
| Формат pwrap-команд vs дамп с телефона / pwrap command format vs phone dump | подтверждён / confirmed |

---

## Границы метода / Limits of the method

- Имена функций отсутствуют — повествование **восстановлено**, это интерпретация.
  No function names exist — the narrative is **reconstructed**, it is interpretation.
- LK собран для **всего** телефона: дисплей, USB, MSDC, PMIC, загрузка.
  Функция может работать для одной подсистемы и ломать другую.
  The LK is built for the **whole** phone: display, USB, MSDC, PMIC, boot.
  A function may work for one subsystem and break another.
- Часть «последовательностей» — скомпилированный общий код,
  повторяющийся в нескольких местах.
  Some "sequences" are common compiled code repeated in several places.

---

## Связанное / Related

- Живые дампы / live dumps: [`android-live-dumps.md`](android-live-dumps.md)
- Журнал SD-карты / SD card log: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Решение по дисплею / display decision: [`../decisions/0001-custom-display-driver.md`](../decisions/0001-custom-display-driver.md)