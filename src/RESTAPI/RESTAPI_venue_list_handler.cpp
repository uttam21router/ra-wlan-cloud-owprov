//
// Created by stephane bourque on 2021-08-23.
//

#include "RESTAPI_venue_list_handler.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "RESTAPI/RESTAPI_rbac_helpers.h"
#include "StorageService.h"

namespace OpenWifi {
	void RESTAPI_venue_list_handler::DoGet() {
        auto RRMvendor = GetParameter("RRMvendor","");
        if (RBAC::IsRootUser(*this)) {
            if(RRMvendor.empty()) {
                return ListHandler<VenueDB>("venues", DB_, *this);
            }
            VenueDB::RecordVec Venues;
            auto Where = fmt::format(" deviceRules LIKE '%{}%' ", RRMvendor);
            DB_.GetRecords(QB_.Offset, QB_.Limit, Venues, Where, " ORDER BY name ");
            return ReturnObject("venues",Venues);
        }

        VenueDB::RecordVec venues;
        if(RRMvendor.empty()) {
            DB_.GetRecords(QB_.Offset, QB_.Limit, venues);
        } else {
            auto Where = fmt::format(" deviceRules LIKE '%{}%' ", RRMvendor);
            DB_.GetRecords(QB_.Offset, QB_.Limit, venues, Where, " ORDER BY name ");
        }
        VenueDB::RecordVec filtered;
        for (const auto &venue : venues) {
            if (RBAC::IsScopeAllowed(*this, RBAC::TargetScope{venue.entity, venue.info.id})) {
                filtered.push_back(venue);
            }
        }
        return MakeJSONObjectArray("venues", filtered, *this);
    }
} // namespace OpenWifi
