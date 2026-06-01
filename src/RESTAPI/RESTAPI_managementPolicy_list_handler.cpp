//
// Created by stephane bourque on 2021-08-26.
//

#include "RESTAPI_managementPolicy_list_handler.h"

#include "RESTAPI/RESTAPI_db_helpers.h"
#include "RESTAPI/RESTAPI_rbac_helpers.h"
#include "RESTObjects/RESTAPI_ProvObjects.h"
#include "StorageService.h"

namespace OpenWifi {
	void RESTAPI_managementPolicy_list_handler::DoGet() {
		if (RBAC::IsRootUser(*this)) {
			return ListHandler<PolicyDB>("managementPolicies", DB_, *this);
		}

		ProvObjects::ManagementPolicyVec allPolicies;
		DB_.GetRecords(QB_.Offset, QB_.Limit, allPolicies);
		ProvObjects::ManagementPolicyVec filtered;
		for (const auto &policy : allPolicies) {
			if (RBAC::IsScopeAllowed(*this, RBAC::TargetScope{policy.entity, policy.venue})) {
				filtered.push_back(policy);
			}
		}
		return MakeJSONObjectArray("managementPolicies", filtered, *this);
	}
} // namespace OpenWifi
