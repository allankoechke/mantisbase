#include "../../include/mantisbase/core/types.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    MbAppMixin::MbAppMixin(const MantisBase& app)
    : m_app(app) {}

    const MantisBase& MbAppMixin::mbApp() const {
        return m_app;
    }

    const MbLogger& MbAppMixin::logger() const {
        return m_app.logger();
    }
}
