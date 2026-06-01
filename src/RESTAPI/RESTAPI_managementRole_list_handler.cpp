//
// Created by stephane bourque on 2021-08-26.
//

#include "RESTAPI_managementRole_list_handler.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "RESTAPI/RESTAPI_rbac_helpers.h"
#include "StorageService.h"

namespace OpenWifi {
	void RESTAPI_managementRole_list_handler::DoGet() {
		if (RBAC::IsRootUser(*this)) {
			return ListHandler<ManagementRoleDB>("roles", DB_, *this);
		}

		ProvObjects::ManagementRoleVec allRoles;
		DB_.GetRecords(QB_.Offset, QB_.Limit, allRoles);
		ProvObjects::ManagementRoleVec filtered;
		for (const auto &role : allRoles) {
			if (RBAC::IsScopeAllowed(*this, RBAC::TargetScope{role.entity, role.venue})) {
				filtered.push_back(role);
			}
		}
		return MakeJSONObjectArray("roles", filtered, *this);
	}
} // namespace OpenWifi
