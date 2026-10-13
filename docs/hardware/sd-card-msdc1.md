# SD-карта / SD card (MSDC1)

**Статус: ❌ не работает. Это текущий блокер проекта.**
**Status: ❌ not working. This is the project's current blocker.**

Документ написан так, чтобы следующий человек (или следующая сессия) **не повторял**
уже сделанные эксперименты и сразу продолжил с последнего вывода.
Written so that the next person (or session) does **not** repeat the experiments already
done, and continues from the last conclusion.

---

## ЧТО не работает / The symptom

| RU | EN |
|---|---|
| Контроллер: **MSDC1** (в форках MTK — `sdhc2`), база **`0x11240000`** | Controller: **MSDC1** (named `sdhc2` in MTK trees), base **`0x11240000`** |
| Прерывание: `GIC_SPI 135` | Interrupt: `GIC_SPI 135` |
| Карта в телефоне: microSD, 4-бит | Card in the phone: microSD, 4-bit |
| **Симптом**: `mmc info` не выводит данных о карте | **Symptom**: `mmc info` prints nothing about the card |
| **Что видно в логе**: `CRCERR`, `INT = 0x480` | **In the log**: `CRCERR`, `INT = 0x480` |
| **Эталон (Android)**: `CMD8` → `0x1aa` с первой попытки | **Reference (Android)**: `CMD8` → `0x1aa` on the first attempt |

Ключевое наблюдение: **контроллер запускается, команда уходит, карта не отвечает.**

The key observation: **the controller starts, the command goes out, the card does
not answer.**

- `CMD0` (без ответа) проходит отлично — `INT=0x180` (CMDRDY)
  `CMD0` (no response expected) passes fine — `INT=0x180` (CMDRDY)
- `CMD8` (ожидаем ответ) — `INT=0x400`, `RSP=0`, то есть ответа нет
  `CMD8` (response expected) — `INT=0x400`, `RSP=0`, i.e. no response
- По поллеру v219: во второй попытке `CMD=1` (`0x1010002`) на первых сэмплах, потом
  линии прижимаются к нулю. В первой попытке `DAT0` поднимается к сэмплу 2–3 —
  **линия живая, но ответа нет**
  Per the v219 poller: on the second attempt `CMD=1` (`0x1010002`) on the first
  samples, then the lines are pulled low. On the first attempt `DAT0` rises by
  sample 2–3 — **the line is alive, but there is no response**

---

## С чем сравнивали / Reference

Всё сверялось с **живым Android**, где карта работает.
Everything was compared against **live Android**, where the card works.

| Источник / Source | Что взято / What was taken |
|---|---|
| `/sys/firmware/fdt` | реальные `compatible`-строки / real `compatible` strings |
| `/sys/kernel/debug/*`, `/sys/class/*` | состояние контроллера / controller state |
| живой дамп регистров | bind-эталоны, до и после команд / bind references, before and after commands |
| декомпилированный LK | последовательность инициализации 1:1 / 1:1 init sequence |
| `.ko` модуль вендора | раскладка регистров (`mtk-mmc.h`) / register layout |

> ⚠️ **Важный вывод разбора:** «отличия» `PATCH_BIT1 = 0xffe64309` и `DTOC`
> оказались **ложными следами** — они наблюдаются только 1–2 мс при reset
> (autok-тюнинг вендора), а не как реальные расхождения.
>
> ⚠️ **Important finding:** the apparent `PATCH_BIT1 = 0xffe64309` and `DTOC`
> differences are **red herrings** — they are only visible for 1–2 ms during reset
> (vendor auto-tuning), not as real divergences.

---

## Хронология попыток / Experiment log

Каждая строка — коммит в `drivers/mmc/mtk-sd.c`. **Не повторять.**
Each row is a commit in `drivers/mmc/mtk-sd.c`. **Do not repeat.**

| Версия | Что меняли / What changed | Результат / Result |
|---|---|---|
| **v161** | исправлен маппинг ID часов в topckgen, маркеры в probe | CLK настроены, карта не видна / clocks set up, card not seen |
| **v162** | отключено включение `h_clk` | обход зависания шины `infra_ao1` / works around `infra_ao1` bus hang |
| **v168** | отключены **все** гейты `infra_ao` в `ungate_clock` | обход зависания шины / bus hang workaround |
| **v174** | флаг `MMC_BROKEN_CD` — всегда «карта есть» | костыль, карта всё равно не отвечает / hack, card still silent |
| **v175–v176** | отладочная печать в `mmc_get_op_cond` | печать ломает USB-перечисление → откат в v177 / printing breaks USB enumeration → reverted in v177 |
| **v177** | отладочная печать удалена | USB снова работает / USB works again |
| **v178** | режим пинов MSDC1 в probe драйвера | без эффекта / no effect |
| **v179** | полный pinctrl MSDC1 (mode + IES + SMT + PUPD + R0/R1) | линии живые, ответа нет / lines alive, no response |
| **v189** | **обнаружение** карты: гейты через CLR + полярность CD | `mmc info` перестал врать, но карты нет / no longer lies, but still no card |
| **v191** | pwrap init + реальные питания PMIC (2.9 В / 1.8 В), MSDC1 src = 192 МГц | ⚠️ 26 МГц был **ошибкой** — на шине 2.8 кГц вместо 400 кГц / 26 MHz was **wrong** — 2.8 kHz on the bus instead of 400 kHz |
| **v214** | последовательность LK 1:1, живые значения Android: `PATCH_BIT 0x403c0007 / 0xfffe4340 / 0x3488180f`, `ADV_CFG0 0x3808c169`, `EMMC51_CFG0 0x2d000000`, `TOP ENH_EN`, `PAD_TUNE=0`, 100 кГц, pinctrl вендора (CLK pull-down, DRV=3), `r_smpl=1` | CRCERR |
| **v218** | `PATCH_BIT2 = 0x2D0B4000` (биты 8,12 как у Android) | CRCERR тот же (`0x480`) / same CRCERR |
| **v219** | `f_min` 400 кГц, `r_smpl=0` (как в живом Android), отключён LK eMMC-boot handshake, `PATCH_BIT2 = 0x348a180f`, `EMMC_CFG1 = 0x2d0b4000`, `async_fifo_crcsts` + `enhance_rx`, команда `msdcbg` | CRCERR тот же. **Регистры 1:1 с bind-эталоном Android** / same CRCERR. **Registers 1:1 with the Android bind reference** |

### Что уже исключено / Ruled out

| Фактор / Factor | Проверено / Tested | Итог / Verdict |
|---|---|---|
| `f_min` | 100 / 200 / 400 кГц | **не влияет** / **no effect** |
| `r_smpl` (RSPL) | 0 и 1 | **не влияет** |
| `DTOC` | 2 и 3 (вендор ставит 3) | **не влияет** |
| `PATCH_BIT2` | `0x3488180f`, `0x348a180f`, `0x2D0B4000` | **не влияет** |
| LK eMMC-boot handshake | вкл. / выкл. / on, off | **не влияет** |
| Пинмux / подтяжки / pinmux, pull-ups | полный набор + вендорский pinctrl / full set plus vendor pinctrl | линии живы / lines alive |
| Питание PMIC (pwrap) | 2.9 В / 1.8 В по живому дампу / per live dump | **не влияет** |
| Тактовая частота / clock rate | 26 МГц (ошибка) → 192 МГц / 26 MHz (wrong) → 192 MHz | исправлено, но не помогло / fixed, but did not help |

> **Вывод v219: статика регистров исчерпана.** Проблема **не** в значениях
> регистров — контроллер настроен 1:1 как в Android, где карта работает.
>
> **Conclusion of v219: static register values are exhausted.** The controller is
> configured 1:1 as in working Android, so the problem is *not* in register values.

---

## Два оставшихся варианта / Two remaining possibilities

Из заметки `mt6789_msdc1_android_init_phase_2026-08-10`:

From the note `mt6789_msdc1_android_init_phase_2026-08-10`:

1. **Команда физически не доходит до карты** — прижимается к низу, несмотря на живой
   `DAT0` в первой попытке.
   **The command physically never reaches the card** — pulled low despite the live
   `DAT0` on the first attempt.
2. **Карта отвечает, но контроллер не защёлкивает ответ** — чисто аппаратная
   (или clocksource) проблема.
   **The card answers but the controller does not latch the response** — a purely
   hardware (or clock source) issue.

Различить их — задача следующего шага.
Telling them apart is the job of the next step.

---

## Следующий шаг / Next step

**Сравнить внутреннее состояние через отладочный мукс MSDC.**

В U-Boot уже есть готовая команда (добавлена в v219):

A ready-made command already exists in U-Boot (added in v219):

```bash
msdcbg
```

Она даёт / it yields:

- все регистры MSDC1 (`0x00`…`0x228`) / all MSDC1 registers
- отладочный мукс: `writel(sel, 0xa0)` → `readl(0xa4)` для `sel = 0..0x63`
  debug mux: write `sel` to `0xa0`, read `0xa4`, for `sel = 0..0x63`
- режим `EMMC50_CFG4`: `sel = 0x27`, затем `writel(i, 0x220)` → `readl(0xa4)` для `i = 0..11`
  `EMMC50_CFG4` mode: `sel = 0x27`, then write `i` to `0x220`, read `0xa4`, for `i = 0..11`

Зеркалит вендорский `msdc_dump_dbg_register()` (`mtk-mmc-dbg.c`).
It mirrors the vendor `msdc_dump_dbg_register()` (`mtk-mmc-dbg.c`).

Текущее состояние в U-Boot: `sel 0x07 = 1`, `sel 0x19 = 1`, остальные нули.
Current state in U-Boot: `sel 0x07 = 1`, `sel 0x19 = 1`, the rest zero.

**Что сделать / What to do:**

1. Снять `msdcbg` на **живом Android** с вставленной картой (там она работает) —
   сопоставить с `/sys/kernel/debug/` или модулем `mtk-mmc-dbg`.
   Capture `msdcbg` on **live Android** with the card inserted (it works there) —
   compare against `/sys/kernel/debug/` or the `mtk-mmc-dbg` module.
2. Снять `msdcbg` в U-Boot с той же картой.
   Capture `msdcbg` in U-Boot with the same card.
3. Сравнить по битам — разница укажет, где расходится внутреннее состояние.
   Compare bit by bit — the difference will show where the internal state diverges.

Команда в U-Boot (вручную, если `msdcbg` не собран):

Command in U-Boot (by hand, if `msdcbg` is not built in):

```
writel 0x112400a0 0x07
md.l 0x112400a4 1
```

---

## Известные грабли / Known traps

| Грабли / Trap | Что делать / What to do |
|---|---|
| Отладочная печать ломает USB-перечисление | печатать через `early_log`, а не `printf` / use `early_log`, not `printf` |
| Запись в `CLR`-регистр `0x1000108c` **вешает шину** | не трогать без понимания; см. `msdc1_clock_bus_hang.md` / do not touch without understanding |
| `source = 26 МГц` даёт 2.8 кГц на шине, а не 26 МГц | использовать 192 МГц (`MSDCPLL`) / use 192 MHz |
| LK eMMC-boot handshake может держать CMD/DAT прижатыми | в v219 отключён, но не помогло / disabled in v219, did not help |
| `MSDC30_1_SEL` требует апдейта битом 29 | забытый `upd` = мукс не переключился / a forgotten `upd` means the mux never switched |

---

## Ранний лог / Early log

Все `EARLY_W`/`EARLY_WF` пишут в буфер по `0x43FFF000` (16 МБ) — читается после
загрузки U-Boot, когда обычная консоль ещё не работает:

All `EARLY_W`/`EARLY_WF` calls write into a 16 MiB buffer at `0x43FFF000` — readable
after U-Boot has booted, when the normal console does not work yet:

```bash
# в консоли U-Boot / in the U-Boot console
md.b 0x43fff010 800
```

Подробно / details: [`../decisions/0004-early-log-ring-buffer.md`](../decisions/0004-early-log-ring-buffer.md)

---

## Связанное / Related

- Часы и гейт `IFRAO_MSDC1` / clocks and the `IFRAO_MSDC1` gate: [`clocks.md`](clocks.md)
- Выбор драйвера / driver choice: [`../decisions/0003-mmc-on-mt8183-compat.md`](../decisions/0003-mmc-on-mt8183-compat.md)
- Исходные заметки / raw notes: `u-boot-x676b_docs/AGENTS/` — `sd_card_support.md`,
  `msdc1_clock_bus_hang.md`, `msdc1_pinmux-critical-fix.md`,
  `mt6789_msdc1_android_init_phase_2026-08-10.md` (последняя / latest),
  `mt6789_msdc1_android_live_dump_2026-08-09.md`
- Разбор LK / LK analysis: `u-boot-x676b_docs/AGENTS/lk_msdc1_sd_analysis_2026-08-09.md`