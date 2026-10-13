# 0004. Ранний лог в кольцевой буфер памяти / Early log in an in-memory ring buffer

**Статус:** принято / accepted
**Дата:** август 2026

---

## Контекст / Context

Большая часть работы — отладка MMIO, часов и прерываний на ранних этапах загрузки
U-Boot, когда:

Most of the work is debugging MMIO, clocks and interrupts in the early stages of
U-Boot boot, when:

- обычная консоль ещё не поднята (нет DRAM для `printf`-буфера, нет stdio);
  the normal console is not up yet (no DRAM for a `printf` buffer, no stdio);
- `printf` ломает USB-перечисление — это выяснилось в v175–v176, отладка
  MMC печатью «сломала» USB;
  `printf` breaks USB enumeration — discovered in v175–v176, where printing in
  the MMC code "broke" USB;
- UART-кабель физически не подключён.
  no UART cable is physically connected.

Нужен способ **записать сообщение до старта консоли и прочитать его потом**.

We need a way to **record a message before the console starts and read it later**.

---

## Решение / Decision

Добавить `include/early_log.h` — кольцевой буфер в фиксированной памяти,
доступный с самого начала загрузки.

Add `include/early_log.h` — a ring buffer in fixed memory, reachable from the
very start of the boot.

| Параметр / Parameter | Значение / Value |
|---|---|
| База / Base | `0x43FFF000` |
| Размер / Size | 16 МБ / MiB |
| Заголовок / Header | 16 байт: `magic:4` + `idx:4` + `seq:4` + `pad:4` |
| Magic | `0x454C4F47` (в памяти читается как `"GOLE"`) |
| Макет / Layout | `UCLASS`-независимый, только `readl`/`writel` и строки / UCLASS-independent, MMIO plus strings |

API:

```c
EARLY_W("simple string");
EARLY_WF("formatted %s = 0x%lx", name, value);
```

### Что именно сделано правильно / What was done right

**Сброс кэша.** Записи в буфер батчатся по 64-байтным строкам:

Cache flush. Writes to the buffer are batched over 64-byte lines:

```c
for (; a < end_a; a += 64)
        asm volatile("dc cvac, %0" :: "r"(a));
asm volatile("dsb sy");
```

`cvac` очищает до точки монолитности, `dsb` гарантирует завершённость —
иначе читатель по USB увидит мусор.

`cvac` cleans to the point of unification, `dsb` guarantees completion —
otherwise a reader over USB would see garbage.

**Кольцевая запись.** При нехватке места указатель оборачивается на ноль —
буфер не перестаёт работать после заполнения, а просто затирает старое.

Ring behaviour. When full, the pointer wraps to zero — the buffer keeps
working, it just overwrites old data.

**Защита от отсутствия кэша.** Проверка `get_sctlr() & CR_C` — если кэш
выключен, `dc cvac` пропускается.

Guarded against a missing cache. Checks `get_sctlr() & CR_C` — if the cache is
off, `dc cvac` is skipped.

---

## Последствия / Consequences

**Плюсы / Pros**
- Сообщения переживают раннюю фазу загрузки и читаются после неё.
  Messages survive the early boot phase and are readable afterwards.
- **Не ломает USB** — нет вывода на консоль, только запись в память.
  **Does not break USB** — nothing goes to the console, only to memory.
- Не требует инициализированного `gd->serial`.
  No initialised `gd->serial` required.

**Минусы / Cons**
- Занимает 16 МБ DRAM постоянно.
  Occupies 16 MiB of DRAM permanently.
- Буфер затирается при долгой загрузке.
  The buffer is overwritten during a long boot.
- Текст в буфере — ASCII, без структур; разбирать нужно вручную.
  ASCII only, no structure — must be parsed manually.

---

## Как читать / How to read

```bash
# в консоли U-Boot (ACM или UART) — после загрузки
# in the U-Boot console (ACM or UART) - after boot
md.l 0x43fff000 4          # заголовок: magic | idx | seq | pad
md.b 0x43fff010 800        # данные — ASCII
```

```bash
# дамп через mtkclient, если U-Boot не грузится
# dump via mtkclient if U-Boot does not boot
cd /workdir/git/heliog99/mtkclient
python mtk r 0x43fff000 0x40000 ram-dump.bin
```

---

## Где используется / Where it is used

| Файл / File | Что логируется / What is logged |
|---|---|
| `arch/arm/cpu/armv8/cache_v8.c` | создание таблиц страниц, MMU / page table creation, MMU |
| `arch/arm/mach-mediatek/mt6789/init.c` | `dram_init`, `mtk_soc_early_init` |
| `drivers/mmc/mtk-sd.c` | срез PS/INT/SDC_STS/CFG во время CMD8 / PS/INT/SDC_STS/CFG snapshot during CMD8 |
| `drivers/clk/mediatek/clk-mtk.c` | путь вычисления частоты / rate computation path |

---

## Отвергнутые варианты / Rejected alternatives

| Вариант / Option | Почему отвергнут / Why rejected |
|---|---|
| `printf` напрямую | Ломает USB-перечисление; не работает до init stdio / breaks USB enumeration; no stdio yet |
| Запись в ramoops | Только в DRAM, недоступно до её инициализации / DRAM only, unavailable before init |
| Передача по UART | Нужен физический кабель — его нет / needs a physical cable that is not there |
| Отдельный UART-буфер U-Boot | Тот же UART, тот же дефицит железа / same UART, same hardware shortage |

---

## Связанное / Related

- Использование в отладке SD / use in SD debugging: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Карта памяти / memory map: [`../hardware/memory-map.md`](../hardware/memory-map.md)
- Исходный файл / source file: `include/early_log.h`