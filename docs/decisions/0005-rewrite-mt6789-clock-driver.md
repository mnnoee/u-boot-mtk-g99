# 0005. Переписан драйвер часов MT6789 / MT6789 clock driver rewritten

**Статус:** принято / accepted
**Дата:** июль 2026 (коммиты `931705fd25f`, `fd101c08d28`, `053c1dfbabf`)

---

## Контекст / Context

Без тактовых частот не работает ничего: ни дисплей (MMSYS, GCE), ни SD-контроллер
(MSDC1), ни USB. Часы — фундамент, к которому подключено всё остальное.

Without clocks nothing works: neither the display (MMSYS, GCE), nor the SD
controller (MSDC1), nor USB. Clocks are the foundation everything else hangs off.

Ожидалось, что в форке `ellyq/u-boot-mtk-g99` драйвер часов MT6789 есть —
файл `drivers/clk/mediatek/clk-mt6789.c` на месте, три драйвера
(apmixedsys, topckgen, infracfg_ao) объявлены, DTS содержит узлы.

It was expected that the fork has an MT6789 clock driver — the file
`drivers/clk/mediatek/clk-mt6789.c` is present, three drivers (apmixedsys,
topckgen, infracfg_ao) are declared, the DTS contains nodes.

Оказалось — **файл был мёртвым**.

Turned out — **the file was dead**.

---

## Что было сломано / What was broken

### 1. Файл не собирался / The file was never built

```makefile
# drivers/clk/mediatek/Makefile — было / was
//obj-$(CONFIG_TARGET_MT6789) += clk-mt6789.o
```

Комментарий в Makefile — это `#`, а не `//`. Строка была «закомментирована»
не тем синтаксисом и привязана к несуществующему `CONFIG_TARGET_MT6789`.
Драйвер часов MT6789 **не компилировался вообще**.

A Makefile comment is `#`, not `//`. The line was "commented out" with the wrong
syntax and tied to a non-existent `CONFIG_TARGET_MT6789`. The MT6789 clock driver
**never compiled at all**.

### 2. Синтаксическая ошибка в C / A syntax error in C

```c
static int mt6789-infracfg_ao_probe(struct udevice *dev)
```

Дефис в идентификаторе — файл не скомпилировался бы, даже если бы строка
Makefile была активна.

A hyphen in an identifier — the file could not have compiled even with an active
Makefile line.

### 3. Таблицы от другого SoC / Tables from another SoC

222 уникальных `CLK_*` ID и 13 PLL, включая `ARMPLL`, `CCIPLL`, `MPLL` —
которых на MT6789 нет. Файл был смесью таблиц разных чипов.

222 unique `CLK_*` ids and 13 PLLs including `ARMPLL`, `CCIPLL` and `MPLL` —
none of which exist on MT6789. The file was a mixture of tables from different chips.

### 4. Пропуски в нумерации ID / Gaps in id numbering

`fclks`, `fdivs` и `muxes` — три **последовательных** массива, разделенных
смещениями `fdivs_offs`/`muxes_offs`. ID обязаны идти подряд.

`fclks`, `fdivs` and `muxes` are three **consecutive** arrays separated by the
`fdivs_offs`/`muxes_offs` offsets. The ids must be contiguous.

В форке были пропуски (31–36, 54–55), из-за чего индекс выходил за границу:

The fork had gaps (31–36, 54–55), so indices went out of bounds:

| Симптом / Symptom | Последствие / Consequence |
|---|---|
| `CLK_TOP_MSDC30_1_SEL` = 116 при `muxes_offs` = 95 | индекс 21 при 5 муксах → OOB / index 21 with 5 muxes → OOB |
| `CLK_TOP_F26M` = 63 | попал в диапазон factor → `fdivs[62]` при 54 делителях → OOB / fell into the factor range → OOB |

Результат — **неверные частоты у блоков**, то есть неработающие контроллеры.

The result — **wrong clock rates for blocks**, i.e. non-working controllers.

### 5. Неверное описание `MSDC30_1_SEL` / Wrong `MSDC30_1_SEL` description

В форке это был обычный `MUX_GATE(..., 0x88, 8, 3, 15)` — один регион.
На реальном чипе мукс работает через пару **set/clear** и требует
**апдейта** битом 29 в отдельном регистре.

In the fork it was a plain `MUX_GATE(..., 0x88, 8, 3, 15)` — a single register.
On the real chip the mux works through a **set/clear** pair and requires an
**update** by bit 29 in a separate register.

---

## Решение / Decision

Файл переписан: **709 → 191 строку** (включая правки в `dt-bindings`).

The file was rewritten: **709 → 191 lines** (including changes in `dt-bindings`).

```makefile
# стало / now
obj-$(CONFIG_ARCH_MEDIATEK) += clk-mt6789.o
```

| Что / What | Результат / Outcome |
|---|---|
| Сборка / Build | включена / enabled |
| Синтаксис / Syntax | исправлено `mt6789_infracfg_ao_probe` |
| PLL | 10 — только реальные для MT6789 (убраны ARMPLL, CCIPLL, MPLл) / only those real for MT6789 |
| ID | перенумерованы подряд: fclks 0–1, fdivs 2–55, muxes 56–60 / renumbered contiguously |
| `dt-bindings` | −169 / +78 строк / lines |
| `MSDC30_1_SEL` | `mux_set_reg` + `mux_clr_reg` + `upd_reg`, флаг `CLK_MUX_SETCLR_UPD` |
| Макрос `PLL()` | упрощён — убраны поля, которых нет на MT6789 / simplified |
| Логирование / Logging | `EARLY_W` в `probe()` трёх драйверов и в `clk-mtk.c` |

Итог: **10 PLL, 54 делителя, 5 муксов, 68 гейтов**.
Bottom line: **10 PLLs, 54 dividers, 5 muxes, 68 gates**.

---

## Последствия / Consequences

**Плюсы / Pros**
- Часы работают — с этого начали работать дисплей, SD и USB.
  Clocks work — that is where display, SD and USB started working.
- Таблицы соответствуют **реальному** MT6789, а не смеси чипов.
  The tables match the **real** MT6789 rather than a mixture of chips.
- Частоты вычисляются корректно: нет выхода за границы массивов.
  Rates are computed correctly: no out-of-bounds array access.
- Мукс MSDC30_1_SEL переключается по-настоящему.
  The MSDC30_1_SEL mux switches for real.

**Минусы / Cons**
- Таблицы ID теперь **не совпадают** с другими MTK-файлами форка
  (`clk-mt8183.c` и др.) — их нельзя смешивать без проверки.
  The id tables no longer match the fork's other MTK files (`clk-mt8183.c` etc.)
  — they must not be mixed without checking.
- Три драйвера теперь собираются всегда при `CONFIG_ARCH_MEDIATEK`,
  то есть и для других MediaTek-таргетов — проверено, что не мешает.
  The three drivers now always build under `CONFIG_ARCH_MEDIATEK`, i.e. also for
  other MediaTek targets — verified harmless.
- Часть потенциально нужных PLL (APLL, NPUPLL, MFGPLL, TVDPLL) оставлена
  «на всякий случай», но не используется.
  Some potentially useful PLLs are kept "just in case" but unused.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант / Option | Почему отвергнут / Why rejected |
|---|---|
| Дописать форк вместо переписывания | В файле ошибки в трёх независимых местах — править целиком дешевле / errors in three independent places — cheaper to fix wholesale |
| Использовать `clk-mt8183.c` | Другое дерево часов и другой набор гейтов; MSDC1 в нём нет / different clock tree and gates, no MSDC1 |
| Задать частоты вручную из DT | Работает, но теряется управление через `clk` — и SD-инициализация всё равно лезет в драйвер / works but loses `clk` control, and SD init still goes through the driver |
| Оставить только то, что нужно сейчас | Тогда `clk dump` покажет неполную картину и отладка SD станет невозможной / `clk dump` would be incomplete and SD debugging impossible |

---

## Грабли на будущее / Traps to remember

> **ID часов обязаны идти подряд.**
> Любое «пропущенное» число в `mt6789-clk.h` = выход за границу массива =
> случайная частота = неработающий контроллер. Молча, без ошибок компиляции.

> **Clock ids must be contiguous.** Any "skipped" number in `mt6789-clk.h` means
> out-of-bounds array access, a random rate, and a dead controller — silently,
> with no compiler error.

> **Не смешивать `mt6789-clk.h` с другими MTK-файлами.**
> Теперь они используют разные пространства ID.

> **Do not mix `mt6789-clk.h` with the other MTK files.** They now use different
> id spaces.

---

## Хронология / Timeline

| Коммит / Commit | Дата / Date | Что / What |
|---|---|---|
| `931705fd25f` | 2026-07-16 | включена сборка драйвера часов для MT6789 / clock driver build enabled for MT6789 |
| `fd101c08d28` | 2026-07-16 | **v161**: исправлена нумерация ID topckgen, маркеры отладки / topckgen id numbering fixed |
| `73aa712b59b` | 2026-07-17 | **v163**: early_log расширен до 16 МБ / early_log grown to 16 MiB |
| `053c1dfbabf` | 2026-08-06 | **v190**: `MSDC30_1_SEL` через SETCLR_UPD |

---

## Связанное / Related

- Подробности / details: [`../hardware/clocks.md`](../hardware/clocks.md)
- Журнал SD-карты / SD card log: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Исходный файл / source file: `drivers/clk/mediatek/clk-mt6789.c`
- Определения ID / id definitions: `include/dt-bindings/clock/mediatek,mt6789-clk.h`