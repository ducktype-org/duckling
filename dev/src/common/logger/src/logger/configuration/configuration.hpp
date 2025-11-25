#pragma once

namespace logger {
    /**
     * If set, dev logs are enabled.
     * If not set, all dev logs are suppressed.
     */
    extern constinit bool enable_dev_logs;

    /**
     * If set, user logs are enabled.
     * If not set, all user logs are suppressed.
     */
    extern constinit bool enable_user_logs;
}