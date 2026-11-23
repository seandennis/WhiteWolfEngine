#include "Viewport/GizmoSettings.h"

bool FGizmoSettings::IsValid() const
{
	// A comparison with something that is no number is false, so such a step is within no limits.
	const bool bHasValidLocationSnap = LocationSnap >= MinLocationSnap && LocationSnap <= MaxLocationSnap;
	const bool bHasValidRotationSnap = RotationSnap >= MinRotationSnap && RotationSnap <= MaxRotationSnap;
	const bool bHasValidScaleSnap = ScaleSnap >= MinScaleSnap && ScaleSnap <= MaxScaleSnap;
	return bHasValidLocationSnap && bHasValidRotationSnap && bHasValidScaleSnap;
}

EGizmoMode FGizmoSettings::GetNextMode(EGizmoMode InMode)
{
	switch (InMode)
	{
		case EGizmoMode::Translate:
			return EGizmoMode::Rotate;
		case EGizmoMode::Rotate:
			return EGizmoMode::Scale;
		case EGizmoMode::Scale:
		default:
			return EGizmoMode::Translate;
	}
}
