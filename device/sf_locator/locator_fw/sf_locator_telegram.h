#ifndef SF_LOCATOR_TELEGRAM_H
#define SF_LOCATOR_TELEGRAM_H

#include "sf_err.h"

/* Send a Telegram message via HTTPS. No-op (returns SF_OK) if BOT_TOKEN or
 * CHAT_ID is empty. Blocks for the duration of the HTTP request, so call from
 * a non-critical context. */
sf_err_t sf_locator_telegram_send(const char *message);

#endif /* SF_LOCATOR_TELEGRAM_H */
