# 0003. MMC1 на совместимости `mt8183-mmc` / MMC1 bound to `mt8183-mmc`

**Статус:** принято / accepted (работа продолжается)
**Дата:** июль 2026, эксперименты до v219 (2026-08-10)

---

## Контекст / Context

В реальном DT телефона SD-контроллер описан как
`compatible = "mediatek,mt6789-mmc"`.

Формковый `mt6789.dtsi` тоже использовал `mt6789-mmc`.

Но в `drivers/mmc/mtk-sd.c` **строки `mt6789-mmc` нет вообще**:

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
{ .compatible = "mediatek,mt8183-mmc", .data = (ulong)&mt8183_compat },  /* ← единственная подходящая */
```

С реальной строкой контроллер **не поднимется вообще** — драйвер не забиндится.

---

## Решение / Decision

Оставить `mediatek,mt8183-mmc` и развивать `mt8183_compat` под MT6789.

Тот же приём применён к USB: в реальном DT `mt6789-usb20`, а `musb-new`
биндится только на `mt8518-musb` — оставили строку, на которую драйвер
реально подписывается.

**Общее правило проекта:** приоритет — чтобы заработало; «правильная» строка
в dtsi — последняя по значимости. Подробнее: [`../compatibility.md`](../compatibility.md)

### Что именно дописано в `mt8183_compat`

```c
static const struct msdc_compatible mt8183_compat = {
        ...
        .clk_div_bits   = 12,
        .pad_tune0      = true,      /* MTK-специфичная настройка pad'ов */
        .async_fifo     = true,
        .async_fifo_crcsts = true,    /* ← добавлено в v219 */
        .data_tune      = true,
        .busy_check     = true,
        .stop_clk_fix   = true,
        .enhance_rx     = true,      /* ← добавлено в v219 */
};
```

---

## Последствия / Consequences

**Плюсы**
- SD-контроллер вообще поднимается и опрашивается (без этого — ноль).
- Все отладочные точки (`msdcbg`, `EARLY_W`) доступны.

**Минусы**
- Имя `mt8183_compat` вводит в заблуждение — это «наш» набор параметров,
  а не параметры MT8183.
- Реализацию для настоящего `mt6789-mmc` всё равно придётся писать,
  если появится драйвер.

**Статус:** SD-карта **не работает**. См. [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
— там полный журнал v161…v219 и вывод, что статика регистров исчерпана.

---

## Отвергнутые варианты / Rejected alternatives

| Вариант | Почему отвергнут |
|---|---|
| Написать строку `mt6789-mmc` | Драйвер не биндится → контроллер мёртв |
| Взять `mt7987-mmc` | Другая раскладка регистров и делители |
| Свой совместимый узел в X676B-DTS | Лишняя абстракция при уже работающей схеме |
| Оставить как есть и ждать апстрима | Работа встала бы намертво |

---

## Связанное / Related

- Журнал экспериментов: [`../hardware/sd-card-msdc1.md`](../hardware/sd-card-msdc1.md)
- Таблица соответствий: [`../compatibility.md`](../compatibility.md)