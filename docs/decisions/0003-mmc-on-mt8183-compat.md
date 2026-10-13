# 0003. MMC1 на совместимости `mt8183-mmc` / MMC1 bound to `mt8183-mmc`

**Статус:** принято / accepted (работа продолжается / work continues)
**Дата:** июль 2026, эксперименты до v219 (2026-08-10)

---

## Контекст / Context

В реальном DT телефона SD-контроллер описан как
`compatible = "mediatek,mt6789-mmc"`.

On the real device the SD controller is described as
`compatible = "mediatek,mt6789-mmc"`.

Формковый `mt6789.dtsi` тоже использовал `mt6789-mmc`.

The fork's `mt6789.dtsi` also used `mt6789-mmc`.

Но в `drivers/mmc/mtk-sd.c` **строки `mt6789-mmc` нет вообще**:

But `drivers/mmc/mtk-sd.c` has **no `mt6789-mmc` string at all**:

```c
{ .compatible = "mediatek,mt7620-mmc", .data = (ulong)&mt7620_compat },
{ .compatible = "mediatek,mt7621-mmc", ... },
{ .compatible = "mediatek,mt7622-mmc", ... },
{ .compatible = "mediatek,mt7623-mmc", ... },
{ .compatible = "mediatek,mt7986-mmc", ... },
{ .compatible = "mediatek,mt7987-mmc", ... },
{ .compatible = "mediatek,mt7981-mmc", ... },
{ .compatible = "mediatek,mt8512-mmc", ... },
{ .compatible = "mediatek,mt8516-mmc", ... },
{ .compatible = "mediatek,mt8183-mmc", .data = (ulong)&mt8183_compat },  /* ← единственная подходящая / the only usable one */
```

С реальной строкой контроллер **не поднимется вообще** — драйвер не забиндится.

With the real string the controller **would not come up at all** — the driver
would not bind.

---

## Решение / Decision

Оставить `mediatek,mt8183-mmc` и развивать `mt8183_compat` под MT6789.

Keep `mediatek,mt8183-mmc` and extend `mt8183_compat` for MT6789.

Тот же приём применён к USB: в реальном DT `mt6789-usb20`, а `musb-new`
биндится только на `mt8518-musb` — оставили строку, на которую драйвер
реально подписывается.

The same approach was applied to USB: the real DT says `mt6789-usb20`, while
`musb-new` binds only to `mt8518-musb` — so the string the driver actually binds
to was kept.

**Общее правило проекта:** приоритет — чтобы заработало; «правильная» строка
в dtsi — последняя по значимости. Подробнее: [`../compatibility.md`](../compatibility.md)

**Project rule overall:** making it work comes first; the "correct" dtsi string
matters least. See [`../compatibility.md`](../compatibility.md).

### Что именно дописано в `mt8183_compat`

```c
static const struct msdc_compatible mt8183_compat = {
        ...
        .clk_div_bits   = 12,
        .pad_tune0      = true,      /* MTK-специфичная настройка pad'ов */
        .async_fifo     = true,
        .async_fifo_crcsts = true,    /* ← добавлено в v219 / added in v219 */
        .data_tune      = true,
        .busy_check     = true,
        .stop_clk_fix   = true,
        .enhance_rx     = true,      /* ← добавлено в v219 / added in v219 */
};
```

---

## Последствия / Consequences

**Плюсы / Pros**
- SD-контроллер вообще поднимается и опрашивается (без этого — ноль).
  The SD controller comes up and is probed at all (without this — nothing).
- Все отладочные точки (`msdcbg`, `EARLY_W`) доступны.
  All debug facilities (`msdcbg`, `EARLY_W`) are available.

**Минусы / Cons**
- Имя `mt8183_compat` вводит в заблуждение — это «наш» набор параметров,
  а не параметры MT8183.
  The name `mt8183_compat` is misleading — it is "our" parameter set, not MT8183's.
- Реализацию для настоящего `mt6789-mmc` всё равно придётся писать,
  если появится драйвер.
  A real `mt6789-mmc` implementation will still be needed if a driver appears.

**Статус:** SD-карта **не работает**. См.
[`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
— там полный журнал v161…v219 и вывод, что статика регистров исчерпана.

**Status:** the SD card **does not work**. See
[`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
— full v161…v219 log and the conclusion that static register values are exhausted.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант / Option | Почему отвергнут / Why rejected |
|---|---|
| Написать строку `mt6789-mmc` | Драйвер не биндится → контроллер мёртв / driver does not bind, controller dead |
| Взять `mt7987-mmc` | Другая раскладка регистров и делители / different register layout and dividers |
| Свой совместимый узел в X676B-DTS | Лишняя абстракция при уже работающей схеме / needless abstraction over a working scheme |
| Оставить как есть и ждать апстрима | Работа встала бы намертво / the work would stall completely |

---

## Связанное / Related

- Журнал экспериментов / experiment log: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Часы и `IFRAO_MSDC1` / clocks: [`../hardware/clocks.md`](../hardware/clocks.md)
- Таблица соответствий / compatibility matrix: [`../compatibility.md`](../compatibility.md)