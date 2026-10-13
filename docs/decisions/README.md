# Решения / Architecture decision records

Здесь — **почему** код выглядит именно так. Формат каждой записи: контекст →
решение → последствия → что пробовали и отвергли.

Here is **why** the code looks the way it does. Each record: context → decision →
consequences → what was tried and rejected.

---

## Список / Index

| # | Решение / Decision | Статус / Status | Файл / File |
|---|---|---|---|
| 0001 | Собственный драйвер дисплея вместо DRM<br>Custom display driver instead of DRM | принято / accepted | [0001](0001-custom-display-driver.md) |
| 0002 | USB только в режиме PIO, без DMA<br>USB PIO-only, no DMA | принято / accepted | [0002](0002-usb-pio-only.md) |
| 0003 | MMC1 на совместимости `mt8183-mmc`<br>MMC1 bound to `mt8183-mmc` | принято / accepted | [0003](0003-mmc-on-mt8183-compat.md) |
| 0004 | Ранний лог в кольцевой буфер памяти<br>Early log into an in-memory ring buffer | принято / accepted | [0004](0004-early-log-ring-buffer.md) |
| 0005 | Переписан драйвер часов MT6789<br>MT6789 clock driver rewritten | принято / accepted | [0005](0005-rewrite-mt6789-clock-driver.md) |

---

## Шаблон для новых записей / Template for new records

```markdown
# NNNN. Короткое название / Short title

**Статус / Status:** предложено / принято / отменено (proposed / accepted / rejected)
**Дата / Date:** ГГГГ-ММ-ДД

## Контекст / Context
Что не так и какие ограничения.

## Решение / Decision
Что решили сделать.

## Последствия / Consequences
- Плюсы
- Минусы
- Что придётся делать дальше

## Отвергнутые варианты / Rejected alternatives
Что пробовали и почему не подошло.
```

---

## Связанное / Related

- Таблица соответствий: [`../compatibility.md`](../compatibility.md)
- Описание железа: [`../hardware/`](../hardware/)
- Полный журнал коммитов: `git log --oneline` в корне репозитория

Compatibility matrix: [`../compatibility.md`](../compatibility.md)
Hardware description: [`../hardware/`](../hardware/)
Full commit log: `git log --oneline` at the repository root